#pragma once
#include <llfio.hpp>

#include "font2mtsdf/decode.hpp"


namespace llfio = LLFIO_V2_NAMESPACE;


namespace f2m {
	void open_input(llfio::path_view path);
	void open_output(llfio::path_view path);
}

namespace f2m {
	void read_into();
	acma::result<void> write_to_output_from(std::string output_path, sl::uint32_t compression_level, std::vector<std::array<std::byte, decoder::font_texture::size_bytes>> bytes);
}


namespace f2m::impl {
	llfio::mapped_file_handle& in_handle() noexcept;
	std::size_t& in_handle_extent() noexcept;

	llfio::mapped_file_handle& out_handle() noexcept;
	std::size_t& out_handle_extent() noexcept;
}