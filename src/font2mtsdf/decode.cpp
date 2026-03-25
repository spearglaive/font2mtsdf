#include "font2mtsdf/decode.hpp"

#include <span>
#include <cstddef>
#include <streamline/functional/functor/generic_stateless.hpp>
#include <streamline/memory/unique_ptr.hpp>

#include <harfbuzz/hb.h>
#include <sirius/arith/rect.hpp>
#include <msdfgen/core/DistanceMapping.h>
#include <msdfgen/core/ShapeDistanceFinder.h>
#include <msdfgen/core/edge-selectors.h>

#include "font2mtsdf/io.hpp"


namespace f2m::decoder {
	acma::result<std::vector<std::array<std::byte, font_texture::size_bytes>>> decode() {
        std::span<const std::byte> font_file_bytes(reinterpret_cast<std::byte const*>(impl::in_handle().address()), impl::in_handle_extent());
        sl::unique_ptr<hb_blob_t, sl::functor::generic_stateless<hb_blob_destroy>> blob_ptr(hb_blob_create(reinterpret_cast<char const*>(font_file_bytes.data()), font_file_bytes.size(), HB_MEMORY_MODE_DUPLICATE, nullptr, nullptr));
        sl::unique_ptr<hb_face_t, sl::functor::generic_stateless<hb_face_destroy>> face_ptr(hb_face_create(blob_ptr.get(), 0));
        sl::unique_ptr<hb_font_t, sl::functor::generic_stateless<hb_font_destroy>> font_ptr(hb_font_create(face_ptr.get()));

        const unsigned int units_per_em = hb_face_get_upem(face_ptr.get());
        const double scale = 1./units_per_em;
        impl::glyph_context glyph_ctx{
            .pos = {},
            .scale = scale,
        };

        hb_draw_funcs_t* draw_funcs = hb_draw_funcs_create();
        hb_draw_funcs_set_move_to_func     (draw_funcs, ::f2m::impl::move_to,  &glyph_ctx, nullptr);
        hb_draw_funcs_set_line_to_func     (draw_funcs, ::f2m::impl::line_to,  &glyph_ctx, nullptr);
        hb_draw_funcs_set_quadratic_to_func(draw_funcs, ::f2m::impl::quad_to,  &glyph_ctx, nullptr);
        hb_draw_funcs_set_cubic_to_func    (draw_funcs, ::f2m::impl::cubic_to, &glyph_ctx, nullptr);

        const unsigned int glyph_count = hb_face_get_glyph_count(face_ptr.get());
        std::vector<std::array<std::byte, font_texture::size_bytes>> glyphs(glyph_count);
        std::vector<acma::rect<float>> glyph_bounds(glyph_count);
        std::vector<acma::pt2f> glyph_padding(glyph_count);
        acma::errc ret = acma::errc::unknown;

        //#pragma omp taskloop 
        for(unsigned int glyph_id = 0; glyph_id < glyph_count; ++glyph_id){
            msdfgen::Shape shape;
            if(!hb_font_draw_glyph_or_fail(font_ptr.get(), glyph_id, draw_funcs, &shape)) [[unlikely]] {
                //#pragma omp atomic write
                ret = acma::errc::invalid_font_file_format;
                break;
                //#pragma omp cancel taskgroup
            }
            //#pragma omp cancellation point taskgroup

            if (!shape.contours.empty() && shape.contours.back().edges.empty())
                shape.contours.pop_back();
            shape.inverseYAxis = true;
            shape.normalize();
            msdfgen::edgeColoringSimple(shape, 3.0);
            msdfgen::Shape::Bounds b = shape.getBounds();
            constexpr static double font_texture_length_em = font_texture::length_pixels / 16.0;
            acma::pt2f top_left{
                static_cast<float>(std::clamp(b.l, -font_texture_length_em, font_texture_length_em)), 
                static_cast<float>(std::clamp(b.t, -font_texture_length_em, font_texture_length_em))
            };
            acma::pt2f bottom_right{
                static_cast<float>(std::clamp(b.r, -font_texture_length_em, font_texture_length_em)),
                static_cast<float>(std::clamp(b.b, -font_texture_length_em, font_texture_length_em)),
            };
            glyph_bounds[glyph_id] = {
                top_left.x(), top_left.y(),
                bottom_right.x() - top_left.x(), bottom_right.y() - top_left.y()
            };

            std::array<float, font_texture::size_bytes> bitmap;

            //msdfgen::DistanceMapping mapping((msdfgen::Range(font_texture_distance_range)));
            //TODO: true SIMD
            glyph_padding[glyph_id] = static_cast<float>(font_texture::padding_em) - acma::pt2f{top_left.x(), bottom_right.y()};
            acma::pt2f msdf_padding = glyph_padding[glyph_id];//font_texture_padding_em - pt2d{top_left.x(), bottom_right.y()};
            #pragma omp parallel for collapse(2)
            for (std::size_t y = 0; y < font_texture::length_pixels; ++y) {
                for (std::size_t col = 0; col < font_texture::length_pixels; ++col) {
                    std::size_t x = (y % 2) ? font_texture::length_pixels - col - 1 : col;
                    acma::pt2d p = acma::pt2d{x + .5, y + .5} / font_texture::glyph_scale - msdf_padding;//pt2d{bitmap_padding_em, ((bitmap_pixel_length)/ static_cast<double>(glyph_scale)) - (b.t - b.b) - bitmap_padding_em};
                    msdfgen::ShapeDistanceFinder<msdfgen::OverlappingContourCombiner<msdfgen::MultiAndTrueDistanceSelector>> distanceFinder(shape);
                    std::array<double, font_texture::channels> distance = std::bit_cast<std::array<double, font_texture::channels>>(distanceFinder.distance(std::bit_cast<msdfgen::Point2>(p)));
                    float* const bitmap_begin = &bitmap[font_texture::channels * (font_texture::length_pixels * (font_texture::length_pixels - y - 1) + x)];
                    for(std::size_t channel = 0; channel < font_texture::channels; ++channel)
                        bitmap_begin[channel] = static_cast<float>(distance[channel] / font_texture::distance_range + .5);
                }
            }

            msdfErrorCorrection(msdfgen::BitmapRef<float, 4>{bitmap.data(), font_texture::length_pixels, font_texture::length_pixels}, shape, msdfgen::Projection(font_texture::glyph_scale, msdfgen::Vector2(msdf_padding.x(), msdf_padding.y())), msdfgen::Range(font_texture::distance_range));

            for(std::size_t i = 0; i < font_texture::size_bytes; ++i)
                glyphs[glyph_id][i] = static_cast<std::byte>(msdfgen::pixelFloatToByte(bitmap[i]));
        }
        if(ret != acma::errc::unknown) return ret;

		return sl::move(glyphs);
	}
}