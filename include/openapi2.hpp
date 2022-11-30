#pragma once

#include <string>
#include <string_view>

#include <simdjson.h>

#include "util.hpp"

namespace openapi {

enum class RequestMethod {
    POST, PUT, GET, DELETE, PATCH, // common ones
    HEAD, CONNECT, OPTIONS, TRACE, // uncommon ones
    UNKNOWN
};

enum class JsonType {
	Object, Array, Primitive, Reference
};

RequestMethod RequestMethodFromString(std::string_view key);
std::string_view RequestMethodToString(RequestMethod rm);
std::string_view JsonTypeToCppType(std::string_view type, std::string_view format = "");

namespace __detail {

// Returns true if there is no error on this simdjson result.
template <typename T>
inline bool simdjson_noerror(const simdjson::internal::simdjson_result_base<T>& ec) noexcept {
	return ec.error() == simdjson::error_code();
}

template <typename T>
class ListAdaptor final : public simdjson::dom::array {
public:
	class Iterator final : public simdjson::dom::array::iterator {
	public:
		Iterator()
			: simdjson::dom::array::iterator() {}
		Iterator(simdjson::dom::array::iterator&& it)
			: simdjson::dom::array::iterator(std::move(it)) {}
		inline T operator*() const { return T(simdjson::dom::array::iterator::operator*()); }
	};
	using iterator_type = Iterator;

	ListAdaptor()
		: simdjson::dom::array(), _is_valid(false) {}
	ListAdaptor(const simdjson::dom::array& array)
		: simdjson::dom::array(array), _is_valid(true) {}
	inline Iterator begin() const noexcept { return _is_valid ? Iterator(simdjson::dom::array::begin()) : Iterator(); }
	inline Iterator end()   const noexcept { return _is_valid ? Iterator(simdjson::dom::array::end()) : begin(); }
	inline size_t   size()  const noexcept { return _is_valid ? simdjson::dom::array::size() : 0; }
	inline bool     empty() const noexcept { return (size() == 0); }

private:
	bool _is_valid;
};

template <typename T>
class MapAdaptor final : public simdjson::dom::object {
public:
	class Iterator final : public simdjson::dom::object::iterator {
	public:
		using value_type = std::pair<std::string_view, T>;
		Iterator()
			: simdjson::dom::object::iterator() {}
		Iterator(simdjson::dom::object::iterator&& it)
			: simdjson::dom::object::iterator(std::move(it)) {}
		inline const value_type operator*() const { return value_type{this->key(), T(this->value())}; }
	};
	using iterator_type = Iterator;
	MapAdaptor()
		: simdjson::dom::object(), _is_valid(false) {}
	MapAdaptor(const simdjson::dom::object& obj)
		: simdjson::dom::object(obj), _is_valid(true) {}
	inline Iterator begin() const noexcept { return _is_valid ? Iterator(simdjson::dom::object::begin()) : Iterator(); }
	inline Iterator end()   const noexcept { return _is_valid ? Iterator(simdjson::dom::object::end()) : begin(); }
	inline size_t   size()  const noexcept { return _is_valid ? simdjson::dom::object::size() : 0; }
	inline bool     empty() const noexcept { return (size() == 0); }

private:
	bool _is_valid;
};

template <typename T>
class OpenAPIObject {
public:
	OpenAPIObject()
		: _json()
		, _is_valid(false) {}
	OpenAPIObject(simdjson::dom::element&& json)
		: _json(json)
		, _is_valid(true) {}
	OpenAPIObject(const OpenAPIObject& other)
		: _json(other._json)
		, _is_valid(other._is_valid) {}
	OpenAPIObject(OpenAPIObject&& other)
		: _json(std::move(other._json))
		, _is_valid(std::move(other._is_valid)) {}
	OpenAPIObject(const simdjson::internal::simdjson_result_base<simdjson::dom::element>& ec) {
		_is_valid = ec.error() == simdjson::error_code();
		_json = _is_valid ? ec.value_unsafe() : decltype(_json)();
	}

	inline operator bool() const noexcept { return _is_valid; }

	std::ostream& operator<<(std::ostream& out) const {
		_json.dump_raw_tape(out);
		return out;
	}

protected:
	template <typename U>
	U _GetObjectIfExist(std::string_view key) const {
		const auto& v = _json.get_object()[key];
		return simdjson_noerror(v) ? U(v) : U();
	}
	template <typename U>
	U _GetValueIfExist(std::string_view key) const {
		const auto& v = _json.get_object()[key];
		return simdjson_noerror(v) ? v.get<U>().value() : U();
	}

	simdjson::dom::element _json;
	bool _is_valid;
};

} // namespace __detail

using StringList = __detail::ListAdaptor<std::string_view>;

struct Property : public __detail::OpenAPIObject<Property> {
	using __detail::OpenAPIObject<Property>::OpenAPIObject;
	std::string_view type() const;
	std::string_view description() const;
	std::string_view pattern() const;
	std::string_view format() const;
	std::string_view reference() const;
	StringList enum_() const;

	// Available if type == array or object
	// Even though items is plural, properties only have one item.
	Property items() const;

	using Properties = __detail::MapAdaptor<Property>;
	Properties properties() const;

	bool IsReference() const noexcept;

	JsonType Print(std::ostream& out, std::string_view name, std::string& indent) const;
};

class ModelSchema;
class ArraySchema;
class Schema : public __detail::OpenAPIObject<Schema> {
public:
	using __detail::OpenAPIObject<Schema>::OpenAPIObject;

	std::string_view description() const;
	std::string_view type() const;
	std::string_view format() const;
	std::string_view ref() const;

	ModelSchema AsModelSchema() const;
	ArraySchema AsArraySchema() const;

	// If the key of this schema is $ref, then this is available.
	std::string_view reference() const;
};

class ModelSchema : public Schema {
public:
	using Schema::Schema;

	struct Property : public __detail::OpenAPIObject<Property> {
		using __detail::OpenAPIObject<Property>::OpenAPIObject;
		std::string_view type() const;
		std::string_view description() const;
		std::string_view pattern() const;

		using Enum = __detail::ListAdaptor<std::string_view>;
		Enum enum_() const;
	};
	using Properties = __detail::MapAdaptor<Property>;
	Properties properties() const;

	StringList required() const;

protected:
	friend Schema;
	explicit ModelSchema(const Schema& schema)
		: Schema(schema) {}
};

class ArraySchema : public Schema {
public:
	using Schema::Schema;
	using Items = __detail::MapAdaptor<ModelSchema>;
	Items items() const;

protected:
	friend Schema;
	explicit ArraySchema(const Schema& schema)
		: Schema(schema) {}
};

class Header : public __detail::OpenAPIObject<Header> {
public:
	using __detail::OpenAPIObject<Header>::OpenAPIObject;

	std::string_view description() const;
	bool required() const;
	bool deprecated() const;
};

class Encoding : public __detail::OpenAPIObject<Encoding> {
public:
	using __detail::OpenAPIObject<Encoding>::OpenAPIObject;

	std::string_view content_type() const;
	std::string_view style() const;
};

class MediaType : public __detail::OpenAPIObject<MediaType> {
public:
	using __detail::OpenAPIObject<MediaType>::OpenAPIObject;

	Schema schema() const;
};

class Response : public __detail::OpenAPIObject<Response> {
public:
	using __detail::OpenAPIObject<Response>::OpenAPIObject;
	std::string_view description() const;
	Schema schema() const;

	using Headers = __detail::MapAdaptor<Header>;
	Headers headers() const;

	using Content = __detail::MapAdaptor<MediaType>;
	Content content() const;
};

class RequestBody : public __detail::OpenAPIObject<RequestBody> {
public:
	using __detail::OpenAPIObject<RequestBody>::OpenAPIObject;

	using Content = __detail::MapAdaptor<MediaType>;
	Content content() const;
	std::string_view description() const;
	bool required() const;
};

class Parameter : public __detail::OpenAPIObject<Parameter> {
public:
	using __detail::OpenAPIObject<Parameter>::OpenAPIObject;

	std::string_view name() const;
	std::string_view in() const;
	std::string_view description() const;
	bool required() const;

	/// If the parameter is a simple type
	std::string_view type() const;
	std::string_view format() const;
	std::string_view pattern() const;

	using Items = __detail::ListAdaptor<Property>;
	Items items() const;

	/// If the parameter is a schema reference
	Schema schema() const;
};

class Operation : public __detail::OpenAPIObject<Operation> {
public:
	using __detail::OpenAPIObject<Operation>::OpenAPIObject;
	std::string_view summary() const;
	std::string_view description() const;
	std::string_view operation_id() const;
	bool deprecated() const;

	using Responses = __detail::MapAdaptor<Response>;
	Responses responses() const;

	using Parameters = __detail::ListAdaptor<Parameter>;
	Parameters parameters() const;

	using Tags = __detail::ListAdaptor<std::string_view>;
	Tags tags() const;
};

class Path : public __detail::OpenAPIObject<Path> {
public:
	using __detail::OpenAPIObject<Path>::OpenAPIObject;

	using Operations = __detail::MapAdaptor<Operation>;
	Operations operations() const { return Operations(_json); }
};

class Server : public __detail::OpenAPIObject<Server> {
public:
	using __detail::OpenAPIObject<Server>::OpenAPIObject;

	class Variable : public __detail::OpenAPIObject<Variable> {
	public:
		using __detail::OpenAPIObject<Variable>::OpenAPIObject;

		std::string_view default_() const;
		std::string_view description() const;
	};

	std::string_view url() const;
	std::string_view description() const;

	using Variables = __detail::MapAdaptor<Variable>;
	Variables variables() const;
};

class Info : public __detail::OpenAPIObject<Info> {
public:
	using __detail::OpenAPIObject<Info>::OpenAPIObject;
	std::string_view title() const;
	std::string_view description() const;
	std::string_view terms_of_service() const;
	std::string_view version() const;
};

class OpenAPI2 : private __detail::OpenAPIObject<OpenAPI2> {
public:
	OpenAPI2() noexcept {}
	OpenAPI2(const OpenAPI2&) = delete;
	OpenAPI2(OpenAPI2&&) noexcept;

	std::string_view openapi() const;
	Info info() const;

	using Servers = __detail::ListAdaptor<Server>;
	Servers servers() const;

	using Paths = __detail::MapAdaptor<Path>;
	Paths paths() const;

	using Definitions = __detail::MapAdaptor<Property>;
	Definitions definitions() const;

	// Should return false if JSON parsing fails or if file is not an OpenAPI swagger file.
	bool Load(const std::string& path);

	Property GetDefinedSchemaByReference(std::string_view);

private:
	simdjson::dom::parser _parser; // Lifetime of document depends on lifetime of parser, so parser must be kept alive.
	simdjson::dom::element _root;
};

// Synthesize a function name give a path and its verb.
// Use this to get a C++-compatible function name when the globally unique operationId is unavailable.
std::string SynthesizeFunctionName(std::string_view pathstr, RequestMethod verb);

} // namespace openapi
