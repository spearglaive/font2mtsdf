#pragma once
#include <exception>

#include <sirius/core/error.hpp>


namespace f2m {
	[[noreturn]] void exit_cause(std::exception const& err) noexcept;
	[[noreturn]] void exit_cause(acma::errc err) noexcept;
}


