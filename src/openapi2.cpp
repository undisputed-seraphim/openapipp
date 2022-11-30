#include "openapi2.hpp"
#include "util.hpp"

using namespace std::literals;

namespace openapi {

constexpr auto def_refstr = "#/definitions/"sv;

RequestMethod RequestMethodFromString(std::string_view key) {
    if (key == "post" || key == "POST") { return RequestMethod::POST; }
    if (key == "put" || key == "PUT") { return RequestMethod::PUT; }
    if (key == "get" || key == "GET") { return RequestMethod::GET; }
    if (key == "delete" || key == "DELETE") { return RequestMethod::DELETE; }
    if (key == "patch" || key == "PATCH") { return RequestMethod::PATCH; }
    if (key == "head" || key == "HEAD") { return RequestMethod::HEAD; }
    if (key == "connect" || key == "CONNECT") { return RequestMethod::CONNECT; }
    if (key == "options" || key == "OPTIONS") { return RequestMethod::OPTIONS; }
    if (key == "trace" || key == "TRACE") { return RequestMethod::TRACE; }
    return RequestMethod::UNKNOWN;
}

std::string_view RequestMethodToString(RequestMethod rm) {
    switch (rm) {
    case RequestMethod::CONNECT: return "connect";
    case RequestMethod::DELETE:  return "delete";
    case RequestMethod::GET:     return "get";
    case RequestMethod::HEAD:    return "head";
    case RequestMethod::OPTIONS: return "options";
    case RequestMethod::PATCH:   return "patch";
    case RequestMethod::POST:    return "post";
    case RequestMethod::PUT:     return "put";
    case RequestMethod::TRACE:   return "trace";
    default: break;
    }
    return "unknown";
}

// Only for simple types.
std::string_view JsonTypeToCppType(std::string_view type, std::string_view format) {
    if (type == "string") {
        return "std::string";
    }
    if (type == "number") {
        if (format == "double") {
            return "double";
        }
        return "float";
    }
    if (type == "boolean") {
        return "bool";
    }
    if (type == "integer") {
        if (format == "int64") {
            return "int64_t";
        }
        return "int32_t";
    }
    return "void*"; // Unknown type (possibly 'object')
};

// TODO: Investigate the exact relationship between Property objects, Item objects, and the objects that contain these subobjects.
// Property objects and Item objects are possibly mutually recursive.
// It's also possible that Schema objects are Item objects with a slightly different form.

std::string_view Property::type() const { return _GetValueIfExist<std::string_view>("type"); }
std::string_view Property::description() const { return _GetValueIfExist<std::string_view>("description"); }
std::string_view Property::pattern() const { return _GetValueIfExist<std::string_view>("pattern"); }
std::string_view Property::format() const { return _GetValueIfExist<std::string_view>("format"); }
std::string_view Property::reference() const { return _GetValueIfExist<std::string_view>("$ref"); }
StringList Property::enum_() const { return _GetObjectIfExist<StringList>("enum"); }
Property Property::items() const { return _GetObjectIfExist<Property>("items"); }
Property::Properties Property::properties() const { return _GetObjectIfExist<Property::Properties>("properties"); }
bool Property::IsReference() const noexcept {
	return reference().starts_with(def_refstr);
}

JsonType Property::Print(std::ostream& out, std::string_view name_, std::string& indent) const {
	std::string name = sanitize(name_);
	write_multiline_comment(out, description(), indent);
	if (this->IsReference()) {
		auto ref = this->reference();
		ref.remove_prefix(def_refstr.size());
		out << indent << ref << " obj;\n";
		return JsonType::Reference;
	}
	JsonType type = JsonType::Object;
	auto typestr = this->type();
	if (typestr == "object") {
		out << indent << "struct " << name << " {\n";
		indent.push_back('\t');
		for (const auto& [subpropname, subprop] : this->properties()) {
			auto rettype = subprop.Print(out, subpropname, indent);
			if (rettype == JsonType::Object) {
				out << indent << sanitize(subpropname) << ' ' << subpropname << "_;\n";
			}
		}
		indent.pop_back();
		out << indent << "};\n";
		//if (!indent.empty()) {
		//	out << indent << name << ' ' << name_ << "; // <-\n";
		//}
	} else if (typestr == "array") {
		type = JsonType::Array;
		auto item = this->items();
		if (item.IsReference()) {
			auto ref = item.reference();
			ref.remove_prefix(def_refstr.size());
			out << indent << "using " << name << " = std::vector<" << ref << ">;\n";
		} else {
			if (item.type() != "object" && item.type() != "array") {
				// If this is a top-level declaration, then we should use 'using'.
				if (indent.empty()) {
					out << indent << "using " << name << " = " << "std::vector<" << JsonTypeToCppType(item.type()) << ">;\n";
				} else {
					out << indent << "std::vector<" << JsonTypeToCppType(item.type()) << "> " << name << ";\n";
				}
			} else if (item.type() == "array") {
				// TODO
			} else {
				auto nested_typename = std::string(name) + '_';
				item.Print(out, nested_typename, indent);
				if (indent.empty()) {
					out << indent << "using " << name << " = " << "std::vector<" << nested_typename << ">;\n";
				} else {
					out << indent << "std::vector<" << nested_typename << "> " << name << "_;\n";
				}
			}
		}
	} else {
		type = JsonType::Primitive;
		out << indent << JsonTypeToCppType(typestr) << ' ' << name << ";\n";
	}
	return type;
}

std::string_view Schema::description() const { return _GetValueIfExist<std::string_view>("description"); }
std::string_view Schema::type() const { return _GetValueIfExist<std::string_view>("type"); }
std::string_view Schema::format() const { return _GetValueIfExist<std::string_view>("format"); }
std::string_view Schema::ref() const { return _GetValueIfExist<std::string_view>("$ref"); }
ModelSchema Schema::AsModelSchema() const { return ModelSchema(std::move(*this)); }
ArraySchema Schema::AsArraySchema() const { return ArraySchema(std::move(*this)); }
std::string_view Schema::reference() const { return _json.get_string(); }

std::string_view ModelSchema::Property::type() const { return _GetValueIfExist<std::string_view>("type"); }
std::string_view ModelSchema::Property::description() const { return _GetValueIfExist<std::string_view>("description"); }
std::string_view ModelSchema::Property::pattern() const { return _GetValueIfExist<std::string_view>("pattern"); }
ModelSchema::Property::Enum ModelSchema::Property::enum_() const { return _GetObjectIfExist<ModelSchema::Property::Enum>("enum"); }
ModelSchema::Properties ModelSchema::properties() const { return _GetObjectIfExist<ModelSchema::Properties>("properties"); }
StringList ModelSchema::required() const { return _GetObjectIfExist<StringList>("required"); }

ArraySchema::Items ArraySchema::items() const { return _GetObjectIfExist<ArraySchema::Items>("items"); }

std::string_view Header::description() const { return _GetValueIfExist<std::string_view>("description"); }
bool Header::required() const { return _GetValueIfExist<bool>("required"); }
bool Header::deprecated() const { return _GetValueIfExist<bool>("deprecated"); }

std::string_view Encoding::content_type() const { return _GetValueIfExist<std::string_view>("content_type"); }
std::string_view Encoding::style() const { return _GetValueIfExist<std::string_view>("style"); }

Schema MediaType::schema() const { return _GetObjectIfExist<Schema>("schema"); }

std::string_view Response::description() const { return _GetValueIfExist<std::string_view>("description"); }
Response::Headers Response::headers() const { return _GetObjectIfExist<Response::Headers>("headers"); }
Response::Content Response::content() const { return _GetObjectIfExist<Response::Content>("content"); }
Schema Response::schema() const { return _GetObjectIfExist<Schema>("schema"); }

RequestBody::Content RequestBody::content() const { return _GetObjectIfExist<RequestBody::Content>("content"); }
std::string_view RequestBody::description() const { return _GetValueIfExist<std::string_view>("description"); }
bool RequestBody::required() const { return _GetValueIfExist<bool>("required"); }

std::string_view Parameter::name() const { return _GetValueIfExist<std::string_view>("name"); }
std::string_view Parameter::in() const { return _GetValueIfExist<std::string_view>("in"); }
std::string_view Parameter::description() const { return _GetValueIfExist<std::string_view>("description"); }
bool Parameter::required() const { return _GetValueIfExist<bool>("required"); }
Schema Parameter::schema() const { return _GetObjectIfExist<Schema>("schema"); }
std::string_view Parameter::type() const { return _GetValueIfExist<std::string_view>("type"); }
std::string_view Parameter::format() const { return _GetValueIfExist<std::string_view>("format"); }
std::string_view Parameter::pattern() const { return _GetValueIfExist<std::string_view>("pattern"); }
Parameter::Items Parameter::items() const { return _GetObjectIfExist<Parameter::Items>("items"); }

std::string_view Operation::summary() const { return _GetValueIfExist<std::string_view>("summary"); }
std::string_view Operation::description() const { return _GetValueIfExist<std::string_view>("description"); }
std::string_view Operation::operation_id() const { return _GetValueIfExist<std::string_view>("operationId"); }
bool Operation::deprecated() const { return _GetValueIfExist<bool>("deprecated"); }
Operation::Responses Operation::responses() const { return _GetObjectIfExist<Operation::Responses>("responses"); }
Operation::Parameters Operation::parameters() const { return _GetObjectIfExist<Operation::Parameters>("parameters"); }
Operation::Tags Operation::tags() const { return _GetObjectIfExist<Operation::Tags>("tags"); }

std::string_view Server::Variable::default_() const { return _GetValueIfExist<std::string_view>("default"); }
std::string_view Server::Variable::description() const { return _GetValueIfExist<std::string_view>("description"); }

std::string_view Server::url() const { return _GetValueIfExist<std::string_view>("url"); }
std::string_view Server::description() const { return _GetValueIfExist<std::string_view>("description"); }
Server::Variables Server::variables() const { return _GetObjectIfExist<Server::Variables>("variables"); }

std::string_view Info::title() const { return _GetValueIfExist<std::string_view>("title"); }
std::string_view Info::description() const { return _GetValueIfExist<std::string_view>("description"); }
std::string_view Info::terms_of_service() const { return _GetValueIfExist<std::string_view>("terms_of_service"); }
std::string_view Info::version() const { return _GetValueIfExist<std::string_view>("version"); }

OpenAPI2::OpenAPI2(OpenAPI2&& other) noexcept
	: _parser(std::move(other._parser))
	, _root(std::move(other._root)) {}

Info OpenAPI2::info() const { return _GetObjectIfExist<Info>("info"); }
OpenAPI2::Servers OpenAPI2::servers() const { return _GetObjectIfExist<OpenAPI2::Servers>("servers"); }
OpenAPI2::Paths OpenAPI2::paths() const { return _GetObjectIfExist<OpenAPI2::Paths>("paths"); }
OpenAPI2::Definitions OpenAPI2::definitions() const { return _GetObjectIfExist<OpenAPI2::Definitions>("definitions"); }

std::string_view OpenAPI2::openapi() const { return _GetObjectIfExist<std::string_view>("openapi"); }

// Should return false if JSON parsing fails or if file is not an OpenAPI swagger file.
bool OpenAPI2::Load(const std::string& path) {
	_root = _parser.load(path);
	_json = _root;
	return true;
}

Property OpenAPI2::GetDefinedSchemaByReference(std::string_view reference) {
	if (reference.starts_with(def_refstr)) { reference.remove_prefix(def_refstr.size()); }
	for (const auto& [schemaname, schema] : definitions()) {
		if (schemaname == reference) { return schema; }
	}
	return Property();
}

std::string SynthesizeFunctionName(std::string_view pathstr, RequestMethod verb) {
	auto name = sanitize(pathstr);
	name = std::string(RequestMethodToString(verb)) + '_' + name;
	return name;
}

} // namespace openapi
