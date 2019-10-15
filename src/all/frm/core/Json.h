#pragma once

#include <frm/core/frm.h>
#include <frm/core/FileSystem.h>
#include <frm/core/Serializer.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// Json
// Traversal of a loaded document is a state machine:
// 
//  Json json("json.json") // use Json::Read() if you need to check the return value
//  
//  if (json.find("Value")) {                          // find a value in the DOM root
//     if (json.getType() == Json::ValueType_Number) { // check it's the right type
//        int v = json.getValue<int>();                // retrieve/store the value
//     }
//  }
//  
//  if (json.find("Array")) {
//     if (json.enterArray()) {
//        while (json.next()) {                               // get the next value while one exists
//           if (json.getType() == Json::ValueType_Number) {  // check it's the right type
//              int v = json.getValue<int>();                 // retrieve/store the value
//           }
//        }
//        int n = json.getArrayLength();
//        for (int i = 0; i < n; ++i) {
//           int v = json.getValue<int>(i);  // alternatively access array elements directly
//        }
//
//        json.leaveArray(); // must leave the array before proceeding
//     }
//  }
// 
//
// Creating and modifying a document works in a similar way:
// 
//  Json json; // create an empty document
//  
//  json.beginObject("Object");
//     json.setValue("Boolean", false);
//     json.setValue("Integer", 2);
//     json.beginObject("NestedObject");
//        json.setValue("Boolean", false);
//     json.endObject();
//  json.endObject();
//  
//  json.beginArray("Array");
//     for (int i = 0; i < 4; ++i) {
//        json.pushValue(i);
//    }
//  json.endArray();
//
//  json.beginArray("Array"); // arrays can be modified
//     int n = json.getArrayLength();
//     for (int i = 0; i < n; ++i) {
//        json.setValue(i, 0);
//     }
//  json.endArray();
//  
//  json.beginArray("ArrayOfArrays");
//     for (int i = 0; i < 4; ++i) {
//        json.beginArray(); // array members are anonymous
//           for (int j = 0; j < 4; ++j) {
//              json.pushValue(j);
//  		}
//        json.endArray();
//  json.endArray();
//
//  Json::Write(json, "json.json");
//
// Notes:
// - String ptrs passed as the _name argument for setValue() are assumed to have 
//   a lifetime at least as long as the Json object. String ptrs passed as the 
//   _value argument are copied internally.
////////////////////////////////////////////////////////////////////////////////
class Json
{
	friend class SerializerJson; 

public:

	enum ValueType_
	{
		ValueType_Null,
		ValueType_Object,
		ValueType_Array,
		ValueType_Bool,
		ValueType_Number,
		ValueType_String,

		ValueType_Count
	};
	typedef int ValueType;

	static bool Read(Json& json_, const File& _file);
	static bool Read(Json& json_, const char* _path, int _root = FileSystem::GetDefaultRoot());
	static bool Write(const Json& _json, File& file_);
	static bool Write(const Json& _json, const char* _path, int _root = FileSystem::GetDefaultRoot());
		
	// Read from _path if specified.
	Json(const char* _path = nullptr, int _root = FileSystem::GetDefaultRoot());
	~Json();

 // Traversal

	// Go to a named value in the current object, return false if not found.
	bool        find(const char* _name);
	
	// Go to the next value in the current object/array, return true if not the end of the object/array.
	bool        next();

	// Enter the current object (call immediately after find() or next()), return false if the current value is not an object.
	bool        enterObject();
	// Leave the current object.
	void        leaveObject();

	// Enter the current array (call immediately after find() or next()), return false if the current value is not an array.
	bool        enterArray();
	// Leave the current array.
	void        leaveArray();

	// Reset the traversal state machine.
	void        reset();

 // Introspection

	// Get the type of the current value.
	ValueType   getType() const;

	// Get the name of the current value, return "" if the current value is an array member.
	const char* getName() const;

	// Get the index of the current value.
	int         getIndex() const;
	
	// Get the number of elements in the current array, return -1 if not in an array.
	int         getArrayLength() const;

	// Get the current value. tType must match the type of the current value (i.e. getValue<int>() must be called only if the value type is ValueType_Number).
	// _i permits array access (when in an array). 0 <= _i < getArrayLength().
	// Note that the ptr returned by getValue<const char*> is only valid during the lifetime of the Json object.
	template <typename tType>
	tType       getValue(int _i = -1) const;

	// Get a named value. Equivalent to find(_name) followed by getValue(-1).
	template <typename tType>
	tType       getValue(const char* _name) { FRM_VERIFY(find(_name)); return getValue<tType>(-1); }
		

 // Modification

    // Set the current value, or the _ith element of the current array if _i >= 0.
	template <typename tType>
	void       setValue(tType _value, int _i = -1);

	// Set a named value. If the value already exists this modifies the type and value.
	template <typename tType>
	void       setValue(tType _value, const char* _name);

	// Push _value into the current array.
	template <typename tType>
	void       pushValue(tType _value);

	// Create and enter an object. If the object already exists this has the same effect as a call to find() followed by
	// a call to enterObject(). If _name is nullptr the object is expected to be an array member.
	void       beginObject(const char* _name = nullptr);
	// Leave the current object.
	void       endObject() { leaveObject(); }
	
	// Create and enter an array. If the array already exists this has the effect as a call to find() followed by a 
	// call to enterArray(). If _name is nullptr the array is expected to be an array member.
	void       beginArray(const char* _name = nullptr);
	// Leave the current array.
	void       endArray() { leaveArray(); }

 // Debug

	typedef bool (OnVisit)(Json* _this_, ValueType _vaueType, const char* _valueName, int _valueIndex, int _depth);
	void        visitAll(OnVisit* _onVisit);
	
private:
	struct Impl;
	Impl* m_impl;

};

////////////////////////////////////////////////////////////////////////////////
// SerializerJson
////////////////////////////////////////////////////////////////////////////////
class SerializerJson: public Serializer
{
public:

	SerializerJson(Json& _json_, Mode _mode);

	Json*       getJson() { return m_json; }

	bool        beginObject(const char* _name = nullptr) override;
	void        endObject() override;

	bool        beginArray(uint& _length_, const char* _name = nullptr) override;
	void        endArray() override;

	const char* getName() const override;
	uint32      getIndex() const override;

	bool        value(bool&       _value_, const char* _name = nullptr) override;
	bool        value(sint8&      _value_, const char* _name = nullptr) override;
	bool        value(uint8&      _value_, const char* _name = nullptr) override;
	bool        value(sint16&     _value_, const char* _name = nullptr) override;
	bool        value(uint16&     _value_, const char* _name = nullptr) override;
	bool        value(sint32&     _value_, const char* _name = nullptr) override;
	bool        value(uint32&     _value_, const char* _name = nullptr) override;
	bool        value(sint64&     _value_, const char* _name = nullptr) override;
	bool        value(uint64&     _value_, const char* _name = nullptr) override;
	bool        value(float32&    _value_, const char* _name = nullptr) override;
	bool        value(float64&    _value_, const char* _name = nullptr) override;
	bool        value(StringBase& _value_, const char* _name = nullptr) override;
	
	bool        binary(void*& _data_, uint& _sizeBytes_, const char* _name = nullptr, CompressionFlags _compressionFlags = CompressionFlags_None) override;

private:

	Json* m_json;

	void onModeChange(Mode _mode) override { m_json->reset(); }

}; // class SerializerJson


} // namespace frm
