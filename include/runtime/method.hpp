#ifndef __METHOD_HPP__
#define __METHOD_HPP__

#include <string>
#include <cassert>
#include <memory>
#include "runtime/constantpool.hpp"
#include "class_parser.hpp"
#include "annotation.hpp"

using std::wstring;
using std::shared_ptr;

class InstanceKlass;

/**
 * single Method. save in InstanceKlass.
 */
class Method {
private:
	// InstanceKlass
	shared_ptr<InstanceKlass> klass;
	// method basic
	wstring name;
	wstring descriptor;
	u2 access_flags;

	// constant pool ** use for <code> and so on
//	cp_info **constant_pool;

	// Attributes: 1, 3, 6, 7, 13, 14, 15, 16, 17, 18, 19, 20, 22
	u2 attributes_count;
	attribute_info **attributes;		// 留一个指针在这，就能避免大量的复制了。因为毕竟 attributes 已经产生，没必要在复制一份。只要遍历判断类别，然后分派给相应的 子attributes 指针即可。

	// RuntimeAnnotation
	Parameter_annotations_t *rva = nullptr;		// [1]

	u1 num_RuntimeVisibleParameterAnnotation = 0;
	Parameter_annotations_t *rvpa = nullptr;		// [n]

	// RuntimeTypeAnnotation [of Method]
	u2 num_RuntimeVisibleTypeAnnotations = 0;
	TypeAnnotation *rvta = nullptr;				// [n]

	// Code attribute
	Code_attribute *code = nullptr;			// TODO: !!! 小心！！ Code 属性中还有一份异常表！！
//	LineNumberTable_attribute *lnt = nullptr;					// 用于调试器。
//	LocalVariableTable_attribute *lvt = nullptr;				// 用于调试器。
//	LocalVariableTypeTable_attribute *lvtt = nullptr;			// 用于调试器。
//	StackMapTable_attribute *smt = nullptr;					// 此属性用于虚拟机的类型检查阶段。
	// RuntimeTypeAnnotation [of Code attribute]
	u2 Code_num_RuntimeVisibleTypeAnnotations = 0;
	TypeAnnotation *Code_rvta = nullptr;				// [n]

	Exceptions_attribute *exceptions = nullptr;
	u2 signature_index;
	Element_value *ad = nullptr;
//	MethodParameters_attribute *mp = nullptr;		// in fact, in my vm, this is of no use.	@Deprecated.

public:
	bool is_static() { return (this->access_flags & ACC_STATIC) == ACC_STATIC; }
	bool is_public() { return (this->access_flags & ACC_PUBLIC) == ACC_PUBLIC; }
	bool is_private() { return (this->access_flags & ACC_PRIVATE) == ACC_PRIVATE; }
	bool is_void() { return descriptor[descriptor.size()-1] == L'V'; }
	bool is_main() { return is_static() && is_public() && is_void(); }
	bool is_native() { return (this->access_flags & ACC_NATIVE) == ACC_NATIVE; }
	bool is_abstract() { return (this->access_flags & ACC_ABSTRACT) == ACC_ABSTRACT; }
	bool is_synchronized() { return (this->access_flags & ACC_SYNCHRONIZED) == ACC_SYNCHRONIZED; }
	wstring return_type() { return descriptor.substr(descriptor.find_first_of(L")")+1); }
public:
	bool operator== (const Method & rhs) const {
		return this->name == rhs.name && this->descriptor == rhs.descriptor;
	}
public:
	Method(shared_ptr<InstanceKlass> klass, method_info & mi, cp_info **constant_pool);
	const wstring & get_name() { return name; }
	const wstring & get_descriptor() { return descriptor; }
	const Code_attribute *get_code() { return code; }
	shared_ptr<InstanceKlass> get_klass() { return klass; }
	wstring parse_signature();
	void print() { std::wcout << name << ":" << descriptor; }

	~Method() {
		for (int i = 0; i < attributes_count; i ++) {
			delete attributes[i];
		}
		delete[] attributes;
	}
};



#endif
