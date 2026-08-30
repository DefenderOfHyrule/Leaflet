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

#pragma once

extern "C"
{
#include <switch/services/fs.h>
}

#include <memory>
#include <tuple>
#include <vector>

#include "install/simple_filesystem.hpp"
#include "data/byte_buffer.hpp"

#include "nx/content_meta.hpp"
#include "nx/ipc/leaf_ipc.h"

namespace leaf::install
{
    class Install
    {
        protected:
            const NcmStorageId m_destStorageId;
            bool m_ignoreReqFirmVersion = false;
            bool m_declinedValidation = false;

            std::vector<nx::ncm::ContentMeta> m_contentMeta;
            std::vector<NcmContentId> m_sessionInstalledNcas;

            Install(NcmStorageId destStorageId, bool ignoreReqFirmVersion);

            virtual std::vector<std::tuple<nx::ncm::ContentMeta, NcmContentInfo>> ReadCNMT() = 0;
            bool IsSessionInstalledNca(const NcmContentId& ncaId) const;
            void TrackSessionInstalledNca(const NcmContentId& ncaId);
            void CleanupSessionInstalledNcas();

            virtual void InstallContentMetaRecords(leaf::data::ByteBuffer& installContentMetaBuf, int i);
            virtual void InstallApplicationRecord(int i);
            virtual void InstallTicketCert() = 0;
            virtual void InstallNCA(const NcmContentId &ncaId) = 0;

        public:
            virtual ~Install();

            // set to true by Prepare() when a downgrade is detected.
            // the UI caller should show a confirmation dialog before calling Begin().
            bool downgradeDetected = false;
            u32  downgradeFromVersion = 0;
            u32  downgradeToVersion   = 0;
            // set to true before retrying Prepare() to skip the downgrade check.
            bool m_skipDowngradeCheck = false;
            bool m_suppressReinstallPrompt = false;
            bool reinstallDetected = false;
            bool m_skipReinstallCheck = false;

            virtual void Prepare();
            virtual void Begin();

            virtual u64 GetTitleId(int i = 0);
            virtual NcmContentMetaType GetContentMetaType(int i = 0);
    };
}
