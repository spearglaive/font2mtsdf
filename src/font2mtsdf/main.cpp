#include <argparse/argparse.hpp>
#define SIRIUS_WINDOWING_CAPABILITIES false
#include <sirius/core/initialize.hpp>

#include "font2mtsdf/parser_instance.hpp"
#include "font2mtsdf/exit_cause.hpp"
#include "font2mtsdf/io.hpp"
#include "font2mtsdf/program_info.hpp"


#define RESULT_CATCH(fn) \
if(auto r = fn; !r.has_value()) \
	f2m::exit_cause(r.error());


int main(int argc, char* argv[]) {
	//Parse args
	f2m::parser_instance().add_argument("-o", "--output")
  		.required()
  		.help("specify the output mtsdf file path. Will be written as a .ktx2 file");

	f2m::parser_instance().add_argument("-c", "--compression")
		.default_value(20)
  		.help("specify the lossless compression level for the output file (1-22). Lower levels give faster but worse compression. Values above 20 should be used with caution as they require more memory.")
		.scan<'u', sl::uint32_t>();

	f2m::parser_instance().add_argument("input")
    	.help("specify the input font file. Must be a valid font file (.ttf, .otf, etc.)");


	try {
		f2m::parser_instance().parse_args(argc, argv);
	} catch (std::exception const& err) {
		f2m::exit_cause(err);
	}

	const std::string input_path = f2m::parser_instance().get<std::string>("input");
	const std::string output_path = f2m::parser_instance().get<std::string>("--output");
	const sl::uint32_t compression = f2m::parser_instance().get<sl::uint32_t>("--compression");
	
	try {
		f2m::open_input(input_path.data());
		//f2m::open_output(output_path.data());
	} catch (std::exception const& err) {
		f2m::exit_cause(err);
	}


	RESULT_CATCH(acma::intitialize_lib(f2m::program_info::name, f2m::program_info::version));

	auto decode_result = f2m::decoder::decode();
	if(!decode_result.has_value())
		f2m::exit_cause(decode_result.error());
	auto decoded_bytes = *std::move(decode_result);

	RESULT_CATCH(f2m::write_to_output_from(output_path, compression, decoded_bytes));

	return 0;
}