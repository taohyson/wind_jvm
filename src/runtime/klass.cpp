/*
 * klass.cpp
 *
 *  Created on: 2017年11月2日
 *      Author: zhengxiaolin
 */

#include "runtime/klass.hpp"
#include "runtime/field.hpp"
#include "classloader.hpp"
#include "runtime/constantpool.hpp"
#include <utility>
#include <cstring>
#include <sstream>

using std::make_pair;
using std::make_shared;
using std::wstringstream;

/*===----------------   aux   --------------------===*/
Type get_type(const wstring & name)
{
	assert(name.size() == 1);		// B,C,D,J,S,.... only one char.
	Type type;
	switch (name[0]) {
		case L'Z':{
			type = Type::BOOLEAN;
			break;
		}
		case L'B':{
			type = Type::BYTE;
			break;
		}
		case L'C':{
			type = Type::CHAR;
			break;
		}
		case L'S':{
			type = Type::SHORT;
			break;
		}
		case L'I':{
			type = Type::INT;
			break;
		}
		case L'F':{
			type = Type::FLOAT;
			break;
		}
		case L'J':{
			type = Type::LONG;
			break;
		}
		case L'D':{
			type = Type::DOUBLE;
			break;
		}
		default:{
			std::cerr << "can't get here!" << std::endl;
			assert(false);
		}
	}
	return type;
}

/*===----------------  InstanceKlass  ------------------===*/
void InstanceKlass::parse_methods(const ClassFile & cf)
{
	wstringstream ss;
	for(int i = 0; i < cf.methods_count; i ++) {
		shared_ptr<Method> method = make_shared<Method>(this, cf.methods[i], cf.constant_pool);
		ss << method->get_name() << L":" << method->get_descriptor();		// save way: [name + ':' + descriptor]
		this->methods.insert(make_pair(ss.str(), method));	// add into
		ss.str(L"");		// make empty
	}
#ifdef DEBUG
	std::wcout << "===--------------- (" << this->get_name() << ") Debug Runtime MethodPool ---------------===" << std::endl;
	std::cout << "methods: total " << this->methods.size() << std::endl;
	int counter = 0;
	for (auto iter : this->methods) {
		std::wcout << "  #" << counter++ << "  " << iter.first << std::endl;
	}
	std::cout << "===---------------------------------------------------------===" << std::endl;
#endif
}

void InstanceKlass::parse_fields(const ClassFile & cf)
{
	wstringstream ss;
	// set up Runtime Field_info to transfer Non-Dynamic field_info
	for (int i = 0; i < cf.fields_count; i ++) {
		shared_ptr<Field_info> metaField = make_shared<Field_info>(cf.fields[i], cf.constant_pool);
		ss << metaField->get_name() << L":" << metaField->get_descriptor();
		if((cf.fields[i].access_flags & 0x08) != 0) {	// static field
			this->static_fields_layout.insert(make_pair(ss.str(), make_pair(total_static_fields_bytes, metaField)));
			total_static_fields_bytes += metaField->get_value_size();	// offset +++
		} else {		// non-static field
			this->fields_layout.insert(make_pair(ss.str(), make_pair(total_static_fields_bytes, metaField)));
			total_non_static_fields_bytes += metaField->get_value_size();
		}
		ss.str(L"");
	}

	// alloc to save value of STATIC fields. non-statics are in oop.
	this->static_fields = new uint8_t[total_static_fields_bytes];
	memset(this->static_fields, 0, total_static_fields_bytes);	// bzero!!
#ifdef DEBUG
	std::wcout << "===--------------- (" << this->get_name() << ") Debug Runtime FieldPool ---------------===" << std::endl;
	std::cout << "static Field: " << this->static_fields_layout.size() << "; non-static Field: " << this->fields_layout.size() << std::endl;
	if (this->fields_layout.size() != 0)		std::cout << "non-static as below:" << std::endl;
	int counter = 0;
	for (auto iter : this->fields_layout) {
		std::wcout << "  #" << counter++ << "  name: " << iter.first << ", offset: " << iter.second.first << ", size: " << iter.second.second->get_value_size() << std::endl;
	}
	counter = 0;
	if (this->static_fields_layout.size() != 0)	std::cout << "static as below:" << std::endl;
	for (auto iter : this->static_fields_layout) {
		std::wcout << "  #" << counter++ << "  name: " << iter.first << ", offset: " << iter.second.first << ", size: " << iter.second.second->get_value_size() << std::endl;
	}
	std::cout << "===--------------------------------------------------------===" << std::endl;
#endif
}

void InstanceKlass::parse_superclass(const ClassFile & cf, ClassLoader *loader)
{
	if (cf.super_class == 0) {	// this class = java/lang/Object		// TODO: java.lang.Object 怎么办？是个接口？？
		this->parent = nullptr;
	} else {			// base class
		assert(cf.constant_pool[cf.super_class-1]->tag == CONSTANT_Class);
		wstring super_name = ((CONSTANT_Utf8_info *)cf.constant_pool[((CONSTANT_CS_info *)cf.constant_pool[cf.super_class-1])->index-1])->convert_to_Unicode();
		std::wcout << super_name << std::endl;
		if (loader == nullptr) {	// bootstrap classloader do this
			this->parent = std::static_pointer_cast<InstanceKlass>(BootStrapClassLoader::get_bootstrap().loadClass(super_name));
		} else {		// my classloader do this
			this->parent = std::static_pointer_cast<InstanceKlass>(loader->loadClass(super_name));
		}

		if (this->parent != nullptr) {
			this->next_sibling = this->parent->get_child();		// set this's elder brother	// note: this->parent->child can't pass the compile. because this->parent is okay, but parent->child is visiting Klass in the InstanceKlass. `Protected` is: InstanceKlass can visit [the Klass part] inside of the IntanceKlass object. But here is: InstanceKlass visit the Klass part inside of the InstanceKlass part(this->parent), but then visit the Klass outer class (parent->child). parent variable is inside the InstanceKlass, but point to an outer Klass not in the InstanceKlass. To solve it, only use setters and getters.
			this->parent->set_child(shared_ptr<InstanceKlass>(this, [](InstanceKlass*){}));			// set parent's newest child
			// above ↑ is a little hack. I don't know whether there is a side effect.
		}
	}
#ifdef DEBUG
	std::wcout << "===--------------- (" << this->get_name() << ") Debug SuperClass ---------------===" << std::endl;
	if (cf.super_class == 0) {
		std::cout << "this class is **java.lang.Object** class and doesn't have a superclass." << std::endl;
	} else {
		std::wcout << "superclass:  #" << cf.super_class << ", name: " << this->parent->get_name() << std::endl;
	}
	std::cout << "===-------------------------------------------------===" << std::endl;
#endif
}

void InstanceKlass::parse_interfaces(const ClassFile & cf, ClassLoader *loader)	// interface should also be made by the InstanceKlass!!
{
	for(int i = 0; i < cf.interfaces_count; i ++) {
		// get interface name
		assert(cf.constant_pool[cf.interfaces[i]-1]->tag == CONSTANT_Class);
		wstring interface_name = ((CONSTANT_Utf8_info *)cf.constant_pool[((CONSTANT_CS_info *)cf.constant_pool[cf.interfaces[i]-1])->index-1])->convert_to_Unicode();
		shared_ptr<InstanceKlass> interface;
		if (loader == nullptr) {
			interface = std::static_pointer_cast<InstanceKlass>(BootStrapClassLoader::get_bootstrap().loadClass(interface_name));
		} else {
			interface = std::static_pointer_cast<InstanceKlass>(loader->loadClass(interface_name));
			assert(interface != nullptr);
		}
		assert(interface != nullptr);
		this->interfaces.insert(make_pair(interface_name, interface));
	}
#ifdef DEBUG
	std::wcout << "===--------------- (" << this->get_name() << ") Debug Runtime InterfacePool ---------------===" << std::endl;
	std::cout << "interfaces: total " << this->interfaces.size() << std::endl;
	int counter = 0;
	for (auto iter : this->interfaces) {
		std::wcout << "  #" << counter++ << "  name: " << iter.first << std::endl;
	}
	std::cout << "===------------------------------------------------------------===" << std::endl;
#endif
}

void InstanceKlass::parse_constantpool(const ClassFile & cf, ClassLoader *loader)
{
	shared_ptr<InstanceKlass> this_class(this, [](InstanceKlass*){});
	this->rt_pool = make_shared<rt_constant_pool>(this_class, loader, cf);
#ifdef DEBUG
	std::wcout << "===--------------- (" << this->get_name() << ") Debug Runtime Constant Pool ---------------===" << std::endl;
	this->rt_pool->print_debug();
	std::cout << "===------------------------------------------------------------===" << std::endl;
#endif
}

InstanceKlass::InstanceKlass(shared_ptr<ClassFile> cf, ClassLoader *loader, ClassType classtype) : loader(loader), Klass()/*, classtype(classtype)*/
{
	this->classtype = classtype;
	// this_class (only name)
	assert(cf->constant_pool[cf->this_class-1]->tag == CONSTANT_Class);
	this->name = ((CONSTANT_Utf8_info *)cf->constant_pool[((CONSTANT_CS_info *)cf->constant_pool[cf->this_class-1])->index-1])->convert_to_Unicode();

	// become Runtime methods
	parse_methods(*cf);
	// become Runtime fields
	parse_fields(*cf);
	// super_class
	parse_superclass(*cf, loader);
	// this_class
	this->access_flags = cf->access_flags;
	cur = Loaded;
	// become Runtime interfaces
	parse_interfaces(*cf, loader);
	// become Runtime constant pool	// 必须放在构造函数之外！因为比如我加载 java.lang.Object，然后经由常量池加载了 java.lang.StringBuilder，注意此时 java.lang.Object 没被放到 system_classmap 中！然后又会加载 java.lang.Object，回来的时候会加载两遍 Object ！这是肯定不对的。于是设计成了丑恶的由 classloader 调用...QAQ
//	parse_constantpool(cf, loader);

	// TODO: enum status, Loaded, Parsed...
	// TODO: Runtime constant pool and remove Non-Dynamic cp_pool.
	// TODO: annotations...
	// TODO: java.lang.Class: mirror!!!!
	// TODO: 貌似没对 java.lang.Object 父类进行处理。比如 wait 方法等等...
	// TODO: ReferenceKlass......
	// TODO: Inner Class!!
	// TODO: 补全 oop 的 Fields.

}

shared_ptr<Field_info> InstanceKlass::get_field(const wstring & signature)
{
	shared_ptr<Field_info> target;
	// search in this->fields_layout
	auto iter = this->fields_layout.find(signature);
	if (iter == this->fields_layout.end()) {
		// search in this->static_fields_layout
		iter = this->static_fields_layout.find(signature);
	} else {
		return (*iter).second.second;
	}
	if (iter == this->static_fields_layout.end()) {
		// search in super_interfaces : reference Java SE 8 Specification $5.4.3.2: Parsing Fields
		for (auto iter : this->interfaces) {
			// TODO: 这些都没有考虑过 Interface 或者 parent 是 数组的情况.....感觉应当进行考虑...  虽然 Interface 我设置的默认是 InstanceKlass，不过 parent 可是 Klass...
			target = iter.second->get_field(signature);
			if (target != nullptr)	return target;
		}
		// search in super_class: parent : reference Java SE 8 Specification $5.4.3.2: Parsing Fields
		// TODO: 这里的强转会有问题！需要 Klass 实现一个方法返回这个 Klass 能不能是 InstanceKlass ！！暂时不考虑。等崩溃的时候再说。
		if (this->parent != nullptr)	// this class is not java.lang.Object. java.lang.Object has no parent.
			target = std::static_pointer_cast<InstanceKlass>(this->parent)->get_field(signature);		// TODO: 这里暂时不是多态，因为没有虚方法。所以我改成了 static_pointer_cast。以后没准 InstanceKlass 要修改，需要注意。
		return target;		// nullptr or Real result.
	} else {
		return (*iter).second.second;
	}

}

shared_ptr<Method> InstanceKlass::get_class_method(const wstring & signature)
{
	assert(methods.size() < 1000);
	assert(this->is_interface() == false);		// TODO: 此处的 verify 应该改成抛出异常。
	shared_ptr<Method> target;
	// search in this->methods
		std::wcout << "finding at: " << this->name << " find: " << signature << " and get method: " << std::endl;	// delete
		std::cout << &this->methods << "  size:" << this->methods.size() << std::endl;
	auto iter = this->methods.find(signature);
	if (iter != this->methods.end())	{
		return (*iter).second;
	}
	// search in parent class (parent 既可以代表接口，又可以代表类。如果此类是接口，那么 parent 是接口。如果此类是个类，那么 parent 也是类。parent 完全按照 this 而定。)
	if (this->parent != nullptr)	// not java.lang.Object
		target = std::static_pointer_cast<InstanceKlass>(this->parent)->get_class_method(signature);
	if (target != nullptr)	return target;
	// search in interfaces and interfaces' [parent interface].
	for (auto iter : this->interfaces) {
		target = iter.second->get_interface_method(signature);
		if (target != nullptr)	return target;
	}
	return nullptr;
}

shared_ptr<Method> InstanceKlass::get_interface_method(const wstring & signature)
{
	std::wcout << this->name << std::endl;
	if (this->name != L"java/lang/Object")			// 如果此类是 Object 类的话，默认也算作接口。	// 注意内部的分隔符变成了 '/' ！！
		assert(this->is_interface() == true);		// TODO: 此处的 verify 应该改成抛出异常。
	shared_ptr<Method> target;
	// search in this->methods
	auto iter = this->methods.find(signature);
	if (iter != this->methods.end())	return (*iter).second;
	// search in parent interface
	if (this->name != L"java/lang/Object")
		assert(this->interfaces.size() == 0 || this->interfaces.size() == 1);		// 这里画蛇添足一下。因为接口最多只有一个父接口，且不能 implements 只能 extends。因而应该只有一个 parent Object 而只能有一个 interfaces。因为虽然接口间继承是用 extends 的，但是那也算作接口而不算做父类。
	for (auto iter : this->interfaces) {		// actually at most one element in interfaces.
		std::wcout << this->name << " has interface: " << iter.first << std::endl;	// delete
		std::wcout << this->name << "'s father: " << this->parent->get_name() << std::endl;	// delete
		target = iter.second->get_interface_method(signature);
		if (target != nullptr)	return target;
	}
	// search in parent... Big probability is java.lang.Object...
	if (this->parent != nullptr)	// this is not java.lang.Object
		target = std::static_pointer_cast<InstanceKlass>(this->parent)->get_interface_method(signature);
	if (target != nullptr)	return target;

	return nullptr;
}

/*===---------------    ArrayKlass    --------------------===*/
ArrayKlass::ArrayKlass(int dimension, ClassLoader *loader, ClassType classtype)  : dimension(dimension), loader(loader), Klass()/*, classtype(classtype)*/ {
	this->classtype = classtype;		// 这个变量不能放在初始化列表中初始化，即【不能用初始化列表直接初始化 不在基类构造函数参数列表 中的 基类的 protected 成员。】。会提示：error: member initializer 'classtype' does not name a non-static data member or base class
	// set super class
	this->set_parent(BootStrapClassLoader::get_bootstrap().loadClass(L"java/lang/Object"));
}

/*===---------------  TypeArrayKlass  --------------------===*/
TypeArrayKlass::TypeArrayKlass(Type type, int dimension, ClassLoader *loader, ClassType classtype) : type(type), ArrayKlass(dimension, loader, classtype)
{	// B:byte C:char D:double F:float I:int J:long S:short Z:boolean s: String e:enum c:Class @:Annotation [:Array
	// 1. get name
	wstringstream ss;		// 注：基本类型没有 enum 和 annotation。因为 enum 在 java 编译器处理之后，会被转型为 inner class。而 annotation 本质上就是普通的接口，相当于 class。所以基础类型没有他们。
	for (int i = 0; i < dimension; i ++) {
		ss << L"[";
	}
	switch (type) {
		case Type::BOOLEAN:{
			ss << L"Z";
			break;
		}
		case Type::BYTE:{
			ss << L"B";
			break;
		}
		case Type::CHAR:{
			ss << L"C";
			break;
		}
		case Type::SHORT:{
			ss << L"S";
			break;
		}
		case Type::INT:{
			ss << L"I";
			break;
		}
		case Type::FLOAT:{
			ss << L"F";
			break;
		}
		case Type::LONG:{
			ss << L"J";
			break;
		}
		case Type::DOUBLE:{
			ss << L"D";
			break;
		}
		default:{
			std::cerr << "can't get here!" << std::endl;
			assert(false);
		}
	}
	this->name = ss.str();
	// TODO: 要不要设置 object 的 child ...? 但是 sibling 的话，应该这个 higher 和 lower dimension 应该够 ???
}

ObjArrayKlass::ObjArrayKlass(shared_ptr<InstanceKlass> element_klass, int dimension, ClassLoader *loader, ClassType classtype) : element_klass(element_klass), ArrayKlass(dimension, loader, classtype)
{
	// 1. set name
	wstringstream ss;
	for (int i = 0; i < dimension; i ++) {
		ss << L"[";
	}
	ss << element_klass->get_name();
	this->name = ss.str();
}



