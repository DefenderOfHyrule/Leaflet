/*
Copyright (c) 2017-2018 Adubbz

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <thread>

#include "install/install_xci.hpp"
#include "util/file_util.hpp"
#include "util/title_util.hpp"
#include "util/debug.h"
#include "util/error.hpp"
#include "util/config.hpp"
#include "util/crypto.hpp"
#include "util/install_diagnostics.hpp"
#include "util/util.hpp"
#include "util/lang.hpp"
#include "install/nca.hpp"
#include "ui/MainApplication.hpp"

namespace inst::ui {
    extern MainApplication *mainApp;
}

namespace {
    u8 GetSystemKeyGeneration() {
        if (hosversionAtLeast(21, 0, 0)) return 20;
        if (hosversionAtLeast(20, 0, 0)) return 19;
        if (hosversionAtLeast(19, 0, 0)) return 18;
        if (hosversionAtLeast(18, 0, 0)) return 17;
        if (hosversionAtLeast(17, 0, 0)) return 16;
        if (hosversionAtLeast(16, 0, 0)) return 15;
        if (hosversionAtLeast(14, 0, 0)) return 14;
        if (hosversionAtLeast(13, 0, 0)) return 13;
        if (hosversionAtLeast(12, 1, 0)) return 12;
        if (hosversionAtLeast(11, 0, 0)) return 11;
        if (hosversionAtLeast(10, 0, 0)) return 10;
        if (hosversionAtLeast(9, 1, 0))  return 9;
        if (hosversionAtLeast(9, 0, 0))  return 8;
        if (hosversionAtLeast(8, 1, 0))  return 7;
        if (hosversionAtLeast(7, 0, 0))  return 6;
        if (hosversionAtLeast(6, 2, 0))  return 5;
        if (hosversionAtLeast(6, 0, 0))  return 4;
        if (hosversionAtLeast(5, 0, 0))  return 3;
        if (hosversionAtLeast(4, 0, 0))  return 2;
        if (hosversionAtLeast(3, 0, 0))  return 1;
        return 0;
    }
}

namespace leaf::install::xci
{
    XCIInstallTask::XCIInstallTask(NcmStorageId destStorageId, bool ignoreReqFirmVersion, const std::shared_ptr<XCI>& xci) :
        Install(destStorageId, ignoreReqFirmVersion), m_xci(xci)
    {
        m_xci->RetrieveHeader();
    }

    std::vector<std::tuple<nx::ncm::ContentMeta, NcmContentInfo>> XCIInstallTask::ReadCNMT()
    {
        std::vector<std::tuple<nx::ncm::ContentMeta, NcmContentInfo>> CNMTList;

        for (const HFS0FileEntry* fileEntry : m_xci->GetFileEntriesByExtension("cnmt.nca")) {
            std::string cnmtNcaName(m_xci->GetFileEntryName(fileEntry));
            NcmContentId cnmtContentId = leaf::util::GetNcaIdFromString(cnmtNcaName);
            size_t cnmtNcaSize = fileEntry->fileSize;

            nx::ncm::ContentStorage contentStorage(m_destStorageId);

            LOG_DEBUG("CNMT Name: %s\n", cnmtNcaName.c_str());

            // we install the cnmt nca early to read from it later
            this->InstallNCA(cnmtContentId);
            std::string cnmtNCAFullPath = contentStorage.GetPath(cnmtContentId);

            NcmContentInfo cnmtContentInfo;
            cnmtContentInfo.content_id = cnmtContentId;
            ncmU64ToContentInfoSize(cnmtNcaSize & 0xFFFFFFFFFFFF, &cnmtContentInfo);
            cnmtContentInfo.content_type = NcmContentType_Meta;

            CNMTList.push_back( { leaf::util::GetContentMetaFromNCA(cnmtNCAFullPath), cnmtContentInfo } );
        }
        
        return CNMTList;
    }

    void XCIInstallTask::InstallNCA(const NcmContentId& ncaId)
    {
        const HFS0FileEntry* fileEntry = m_xci->GetFileEntryByNcaId(ncaId);
        std::string ncaFileName = m_xci->GetFileEntryName(fileEntry);
        LOG_DEBUG("Installing %s to storage Id %u\n", ncaFileName.c_str(), m_destStorageId);

        std::shared_ptr<nx::ncm::ContentStorage> contentStorage(new nx::ncm::ContentStorage(m_destStorageId));

        try {
            contentStorage->DeletePlaceholder(*(NcmPlaceHolderId*)&ncaId);
        }
        catch (...) {}

        LOG_DEBUG("Size: 0x%lx\n", fileEntry->fileSize);

        try {
            {
                leaf::install::NcaHeader* header = new NcaHeader;
                m_xci->BufferData(header, m_xci->GetDataOffset() + fileEntry->dataOffset, sizeof(leaf::install::NcaHeader));
                Crypto::AesXtr crypto(Crypto::Keys().headerKey, false);
                crypto.decrypt(header, header, sizeof(leaf::install::NcaHeader), 0, 0x200);

                if (header->magic != MAGIC_NCA3) {
                    delete header;
                    THROW_FORMAT("Invalid NCA magic");
                }

                const u8 contentKeygen = std::max(header->m_cryptoType, header->m_cryptoType2);
                if (contentKeygen > 0) {
                    const u8 systemKeygen = GetSystemKeyGeneration();
                    if (contentKeygen > systemKeygen) {
                        delete header;
                        THROW_FORMAT("This content requires firmware key generation %u, but your system only supports up to %u. Update your firmware to install this title.", (unsigned)contentKeygen, (unsigned)systemKeygen);
                    }
                }


                const bool hasNonZeroRightsId = (header->m_rightsId[0] != 0 || header->m_rightsId[1] != 0);
                if (hasNonZeroRightsId && m_xci->GetFileEntriesByExtension("tik").empty()) {
                    delete header;
                    THROW_FORMAT("Please redump your game using nxdumptool and enable \"nca/tik: remove titlekey crypto\" before dumping.");
                }
                delete header;
            }

            m_xci->StreamToPlaceholder(contentStorage, ncaId);

            LOG_DEBUG("                                                           \r");
            LOG_DEBUG("Registering placeholder...\n");

            try
            {
                contentStorage->Register(*(NcmPlaceHolderId*)&ncaId, ncaId);
            }
            catch (...)
            {
                LOG_DEBUG(("Failed to register " + ncaFileName + ". It may already exist.\n").c_str());
            }

            try
            {
                contentStorage->DeletePlaceholder(*(NcmPlaceHolderId*)&ncaId);
            }
            catch (...) {}
        }
        catch (...)
        {
            try { contentStorage->DeletePlaceholder(*(NcmPlaceHolderId*)&ncaId); } catch (...) {}
            try {
                if (contentStorage->Has(ncaId))
                    contentStorage->Delete(ncaId);
            } catch (...) {}
            throw;
        }
    }

    void XCIInstallTask::InstallTicketCert()
    {
        std::vector<const HFS0FileEntry*> tikFileEntries = m_xci->GetFileEntriesByExtension("tik");
        std::vector<const HFS0FileEntry*> certFileEntries = m_xci->GetFileEntriesByExtension("cert");

        if (tikFileEntries.size() != certFileEntries.size()) {
            THROW_FORMAT("Ticket / Cert missmatch");
        }

        for (size_t i = 0; i < tikFileEntries.size(); i++)
        {
            if (tikFileEntries[i] == nullptr)
            {
                LOG_DEBUG("Remote tik file is missing.\n");
                THROW_FORMAT("Remote tik file is not present!");
            }

            u64 tikSize = tikFileEntries[i]->fileSize;
            auto tikBuf = std::make_unique<u8[]>(tikSize);
            LOG_DEBUG("> Reading tik\n");
            m_xci->BufferData(tikBuf.get(), m_xci->GetDataOffset() + tikFileEntries[i]->dataOffset, tikSize);

            {
                const u32 sigType = tikBuf[0] | (tikBuf[1] << 8) | (tikBuf[2] << 16) | (tikBuf[3] << 24);
                size_t dataOffset = 0;
                if      (sigType == 0x10003 || sigType == 0x10000) dataOffset = 0x240;
                else if (sigType == 0x10004 || sigType == 0x10001) dataOffset = 0x140;
                else if (sigType == 0x10005 || sigType == 0x10002) dataOffset = 0x80; 
                else if (sigType == 0x10006)                       dataOffset = 0x40; 
                if (dataOffset > 0 && tikSize >= dataOffset + 0x142) {
                    const u8 titleKeyType = tikBuf[dataOffset + 0x141];
                    if (titleKeyType == 0x01)
                        THROW_FORMAT("Please redump your game using nxdumptool and enable \"nca/tik: remove titlekey crypto\" before dumping.");
                }
            }

            if (certFileEntries[i] == nullptr)
            {
                LOG_DEBUG("Remote cert file is missing.\n");
                THROW_FORMAT("Remote cert file is not present!");
            }

            u64 certSize = certFileEntries[i]->fileSize;
            auto certBuf = std::make_unique<u8[]>(certSize);
            LOG_DEBUG("> Reading cert\n");
            m_xci->BufferData(certBuf.get(), m_xci->GetDataOffset() + certFileEntries[i]->dataOffset, certSize);

            // finally, let's actually import the ticket
            ASSERT_OK(esImportTicket(tikBuf.get(), tikSize, certBuf.get(), certSize), "Failed to import ticket");
        }
    }
}
