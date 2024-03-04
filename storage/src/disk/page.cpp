#include "disk/page.h"

namespace storage {
size_t Page::HdrOffset = offsetof(Page, hdr);
size_t Page::PayloadOffset = offsetof(Page, payload);

void Page::deserizalize(const char *data) {
    ::memcpy(this + HdrOffset, data, sizeof(PageHdr));
    ::memcpy(payload, data + PayloadOffset, payload_len());
}

tl::expected<std::shared_ptr<char>, ErrorCode> Page::serialize() const {
    std::shared_ptr<char> raw =
        std::shared_ptr<char>(new char[config::PAGE_SIZE]{0});
    ::memcpy(raw.get(), this + HdrOffset, sizeof(PageHdr));
    if (!payload)
        return tl::unexpected(ErrorCode::InvalidPagePayload);
    ::memcpy(raw.get() + PayloadOffset, payload, payload_len());
    return raw;
}
} // namespace storage
