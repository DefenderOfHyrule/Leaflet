#pragma once
#include "install/nsp.hpp"

namespace quark::install {

    class QuarkNSP : public leaf::install::nsp::NSP {
    private:
        std::string m_remotePath;

    public:
        explicit QuarkNSP(std::string remotePath);

        void StreamToPlaceholder(std::shared_ptr<nx::ncm::ContentStorage> &contentStorage,
                                 NcmContentId placeholderId) override;
        void BufferData(void *buf, off_t offset, size_t size) override;
    };

}
