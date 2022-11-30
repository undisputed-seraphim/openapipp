#pragma once

#include <string_view>

void ltrim(std::string_view &s);

void rtrim(std::string_view &s);

void trim(std::string_view &s);

bool compare_ignore_case(std::string_view l, std::string_view r) noexcept;

void sanitize(std::string& input);

std::string sanitize(std::string_view input);

void write_multiline_comment(std::ostream& out, std::string_view comment, std::string_view indent = "");

std::string transform_url_to_function_signature(std::string_view);