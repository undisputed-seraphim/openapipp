#include <filesystem>
#include <fstream>

#include "openapi2.hpp"
#include "util.hpp"

namespace fs = std::filesystem;
using namespace std::literals;

void WriteHeader(std::ostream& out, const openapi::OpenAPI2& file) {
	out << "#pragma once\n"
		<< "#include <nghttp2/nghttp2.h>\n"
		<< "#include <nghttp2/asio_http2.h>\n"
		<< "#include <nghttp2/asio_http2_server.h>\n"
		<< '\n'
		<< "using Request = nghttp2::asio_http2::server::request;\n"
		<< "using Response = nghttp2::asio_http2::server::response;\n"
		<< '\n'
		<< "// This file contains function prototypes for each path/requestmethod pair.\n"
		<< "// Implement the function bodies for each prototype here.\n"
		<< std::endl;

	for (const auto& [pathstr, path] : file.paths()) {
		for (const auto& [opstr, op] : path.operations()) {
			write_multiline_comment(out, op.description());
			out << "void " << op.operation_id() << "(const Request& req, const Response& res);\n\n";
		}
	}
	out << '\n'
		<< "// Call this function to get an instance of a server object with all paths laid out.\n"
		<< "nghttp2::asio_http2::server::http2 add_routes();" << std::endl;
}

void WriteImpl(std::ofstream& out, openapi::OpenAPI2& file) {
	out << "nghttp2::asio_http2::server::http2& add_routes(nghttp2::asio_http2::server::http2& server) {\n";
	for (const auto& [pathstr, path] : file.paths()) {
		for (const auto& [opstr, op] : path.operations()) {
			out << "\t\tif (req.method() == \"" << opstr << "\") {\n"
				<< "\t\t\treturn " << op.operation_id() << "(req, res);\n"
				<< "\t\t}\n";
		}
	}
	out << "\treturn server;\n"
		<< "}\n"
		<< std::endl;
}

void WriteStub(std::ofstream& out, openapi::OpenAPI2& file) {
	for (const auto& [pathstr, path] : file.paths()) {
		for (const auto& [opstr, op] : path.operations()) {
			out << "void " << op.operation_id() << "(const Request& req, const Response& res) {\n"
				<< "\t// Request\n";
			for (const auto& param : op.parameters()) {
				write_multiline_comment(out, param.description(), "\t");
				// out << "\t" << JsonTypeToCppType(parameter["type"].get_string()) << ' ' << parameter["name"].get_string() << ";\n";
			}
			out << "}\n" << std::endl;
		}
	}
}

// Writes header and impl files for nghttp2.
void nghttp2(const fs::path& input, const fs::path& output, openapi::OpenAPI2& file) {
	fs::path paths_header = output / (input.stem().string() + "_paths.hpp");
	fs::path paths_impl = output / (input.stem().string() + "_paths.cpp");
	fs::path paths_stub = output / (input.stem().string() + "_paths_stub.cpp");
	fs::path defs_file = output / (input.stem().string() + "_defs.hpp");

	auto out = std::ofstream(paths_header);
	out << "// DO NOT EDIT. Automatically generated from " << input.filename().string() << '\n';
	WriteHeader(out, file);

	out = std::ofstream(paths_impl);
	out << "// DO NOT EDIT. Automatically generated from " << input.filename().string() << '\n';
	out << "#include \"" << paths_header.filename().string() << "\"\n\n";
	WriteImpl(out, file);

	out = std::ofstream(paths_stub);
	out << "#include \"" << defs_file.filename().string() << "\"\n\n";
	out << "#include \"" << paths_header.filename().string() << "\"\n\n";
	WriteStub(out, file);
}
