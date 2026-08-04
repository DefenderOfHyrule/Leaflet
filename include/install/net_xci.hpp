#pragma once
#include "install/xci.hpp"

namespace quark::net::install {

    class NetXCI : public leaf::install::xci::XCI {
    private:
        std::string m_remotePath;
    public:
        explicit NetXCI(std::string remotePath);
        void StreamToPlaceholder(std::shared_ptr<nx::ncm::ContentStorage> &contentStorage,
                                 NcmContentId placeholderId) override;
        void BufferData(void *buf, off_t offset, size_t size) override;
    };

}
