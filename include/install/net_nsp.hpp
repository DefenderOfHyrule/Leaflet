#pragma once
#include "install/nsp.hpp"

namespace quark::net::install {

    class NetNSP : public leaf::install::nsp::NSP {
    private:
        std::string m_remotePath;
    public:
        explicit NetNSP(std::string remotePath);
        void StreamToPlaceholder(std::shared_ptr<nx::ncm::ContentStorage> &contentStorage,
                                 NcmContentId placeholderId) override;
        void BufferData(void *buf, off_t offset, size_t size) override;
    };

}
