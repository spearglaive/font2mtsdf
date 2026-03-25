#include "font2mtsdf/exit_cause.hpp"
#include <cstdlib>
#include <iostream>

#include "font2mtsdf/parser_instance.hpp"


namespace f2m {
	[[noreturn]] void exit_cause(std::exception const& err) noexcept {
		std::cerr << err.what() << std::endl;
		std::cerr << parser_instance();
		std::exit(1);
	}

	[[noreturn]] void exit_cause(acma::errc err) noexcept {
		std::cerr << acma::error::code_descs.find(err)->second << std::endl;
		std::cerr << parser_instance();
		std::exit(1);
	}
}
