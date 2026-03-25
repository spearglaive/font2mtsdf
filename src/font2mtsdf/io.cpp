#include "font2mtsdf/io.hpp"

#include <streamline/memory/unique_ptr.hpp>

#include <ktx.h>

#include "font2mtsdf/program_info.hpp"


namespace f2m {
	void open_input(llfio::path_view path) {
		llfio::mapped_file_handle mh = llfio::mapped_file({}, path).value();

		std::size_t extent = mh.maximum_extent().value();

		impl::in_handle_extent() = std::move(extent);
		impl::in_handle() = std::move(mh);
	}

	void open_output(llfio::path_view path) {
		llfio::mapped_file_handle mh = llfio::mapped_file({}, path, llfio::mapped_file_handle::mode::write, llfio::mapped_file_handle::creation::always_new).value();


		std::size_t extent = mh.maximum_extent().value();
		
		impl::out_handle_extent() = std::move(extent);
		impl::out_handle() = std::move(mh);
	}
}


namespace f2m {
	void read_into() {

	}

	acma::result<void> write_to_output_from(std::string output_path, sl::uint32_t compression_level, std::vector<std::array<std::byte, decoder::font_texture::size_bytes>> bytes) {

		using texture_unique_ptr = sl::unique_ptr<ktxTexture2, sl::functor::generic_stateless<ktxTexture2_Destroy>>;
		texture_unique_ptr ktx_ptr = sl::make_default<texture_unique_ptr>(sl::in_place_tag);

		const ktxTextureCreateInfo texture_create_info{
			.vkFormat = VK_FORMAT_R8G8B8A8_UNORM,
			.baseWidth = decoder::font_texture::length_pixels,
			.baseHeight = decoder::font_texture::length_pixels,
			.baseDepth = 1,
			.numDimensions = 2,
			.numLevels = 1,
			.numLayers = static_cast<ktx_uint32_t>(bytes.size()),
			.numFaces = 1,
			.isArray = KTX_TRUE,
			.generateMipmaps = KTX_FALSE
		};

		__D2D_KTX_VERIFY(ktxTexture2_Create(&texture_create_info, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &ktx_ptr.get()));
		
		for(sl::index_t i = 0; i < bytes.size(); ++i) {
			__D2D_KTX_VERIFY(ktxTexture_SetImageFromMemory(ktxTexture(ktx_ptr.get()), 0, i, 0, reinterpret_cast<ktx_uint8_t*>(bytes[i].data()), bytes[i].size()));
		}

		const sl::uint32_t compression = std::min(compression_level, static_cast<sl::uint32_t>(22));
		if(compression != 0) {
			__D2D_KTX_VERIFY(ktxTexture2_DeflateZstd(ktx_ptr.get(), compression));
			
			const std::string supercompression_info = std::format("--zcmp {}", compression);
        	__D2D_KTX_VERIFY(ktxHashList_AddKVPair(&ktx_ptr->kvDataHead, KTX_WRITER_SCPARAMS_KEY, supercompression_info.size() + 1, supercompression_info.data()));
		}


		const std::string writer_info = std::format("{} v{}.{}.{}",
			program_info::name,
			program_info::version[0],
			program_info::version[1],
			program_info::version[2]
		);
		__D2D_KTX_VERIFY(ktxHashList_AddKVPair(&ktx_ptr->kvDataHead, KTX_WRITER_KEY, writer_info.size() + 1, writer_info.c_str()));

		
		//TODO: use ktxTexture2_WriteToStream with ktxStream wrapped over llfio instead
		__D2D_KTX_VERIFY(ktxTexture2_WriteToNamedFile(ktx_ptr.get(), output_path.data()));
		return {};
	}
}


namespace f2m::impl {
	llfio::mapped_file_handle& in_handle() noexcept {
		static llfio::mapped_file_handle mh{};
		return mh;
	}

	std::size_t& in_handle_extent() noexcept {
		static std::size_t sz{0};
		return sz;
	}


	llfio::mapped_file_handle& out_handle() noexcept {
		static llfio::mapped_file_handle mh{};
		return mh;
	}

	std::size_t& out_handle_extent() noexcept {
		static std::size_t sz{0};
		return sz;
	}
}
