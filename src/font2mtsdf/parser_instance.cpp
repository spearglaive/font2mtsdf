#include "font2mtsdf/parser_instance.hpp"
#include "font2mtsdf/program_info.hpp"


namespace f2m {
	argparse::ArgumentParser& parser_instance() noexcept {
		const std::string version_string = std::format("{}.{}.{}", 
			program_info::version.major(),
			program_info::version.minor(),
			program_info::version.patch()
		);

		static argparse::ArgumentParser ap(std::string(program_info::name), version_string);
		return ap;
	}
}