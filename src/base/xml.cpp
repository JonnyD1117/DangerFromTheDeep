/*
Danger from the Deep - Open source submarine simulation
Copyright (C) 2003-2020  Thorsten Jordan, Luis Barrancos and others.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

// xml access interface
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#include "xml.hpp"

#include "../tinyxml/tinyxml.h"

#include <cstdio>

#ifdef WIN32
#ifdef _MSC_VER
#define snprintf sprintf_s
#endif
#endif

using std::string;

auto xml_elem::child(const std::string& name) const -> xml_elem
{
    auto* e = elem->FirstChildElement(name);
    if (e == nullptr) {
        THROW(xml_elem_error, name, doc_name());
    }
    return {e};
}

auto xml_elem::has_child(const std::string& name) const -> bool
{
    auto* e = elem->FirstChildElement(name);
    return e != nullptr;
}

auto xml_elem::add_child(const std::string& name) -> xml_elem
{
    auto* e = new TiXmlElement(name);
    elem->LinkEndChild(e);
    return {e};
}

auto xml_elem::doc_name() const -> const std::string&
{
    auto* doc = elem->GetDocument();
    // extra-Paranoia... should never happen
    if (doc == nullptr) {
        THROW(xml_error, std::string("can't get document name for node ") + elem->ValueStr(), "???");
    }

    return doc->ValueStr();
}

auto xml_elem::has_attr(const std::string& name) const -> bool
{
    return elem->Attribute(name) != nullptr;
}

auto xml_elem::attr(const std::string& name) const -> std::string
{
    const auto* tmp = elem->Attribute(name);
    if (tmp != nullptr) {
        return *tmp;
    }
    return {};
}

auto xml_elem::attri(const std::string& name) const -> int
{
    const auto* tmp = elem->Attribute(name);
    if (tmp != nullptr) {
        return atoi(tmp->c_str());
    }
    return 0;
}

auto xml_elem::attru(const std::string& name) const -> unsigned
{
    return unsigned(attri(name));
}

auto xml_elem::attrf(const std::string& name) const -> double
{
    const auto* tmp = elem->Attribute(name);
    if (tmp != nullptr) {
        return atof(tmp->c_str());
    }
    return 0.0;
}

auto xml_elem::attrv3() const -> vector3
{
    return {attrf("x"), attrf("y"), attrf("z")};
}

auto xml_elem::attrv2() const -> vector2
{
    return {attrf("x"), attrf("y")};
}

auto xml_elem::attrv2i() const -> vector2i
{
    return {attri("x"), attri("y")};
}

auto xml_elem::attrq() const -> quaternion
{
    return quaternion(attrf("s"), attrv3());
}

auto xml_elem::attra() const -> angle
{
    return {attrf("angle")};
}

auto xml_elem::attrb(const std::string& name) const -> bool
{
    return attru(name) != 0;
}

void xml_elem::set_attr(const std::string& val, const std::string& name)
{
    elem->SetAttribute(name, val);
}

void xml_elem::set_attr(unsigned u, const std::string& name)
{
    set_attr(int(u), name);
}

void xml_elem::set_attr(int i, const std::string& name)
{
    elem->SetAttribute(name, i);
}

void xml_elem::set_attr(double f, const std::string& name)
{
    // note! DO NOT USE std::ostringstream HERE!
    // its format is different to sprintf(), it has less precision!
    // we could change ostringstream's format, but for what? this is easier...
    char tmp[64];
    int l = snprintf(tmp, 64, "%f", f);

    // strip unneeded zeros at end.
    for (int i = l - 1; i >= 0; --i) {
        if (tmp[i] == '0') {
            tmp[i] = 0;
        } else {
            // strip dot at end, if it remains
            if (tmp[i] == '.') {
                tmp[i] = 0;
            }
            break;
        }
    }
    set_attr(std::string(tmp), name);
}

void xml_elem::set_attr(const vector3& v)
{
    set_attr(v.x, "x");
    set_attr(v.y, "y");
    set_attr(v.z, "z");
}

void xml_elem::set_attr(const vector2& v)
{
    set_attr(v.x, "x");
    set_attr(v.y, "y");
}

void xml_elem::set_attr(const quaternion& q)
{
    set_attr(q.s, "s");
    set_attr(q.v);
}

void xml_elem::set_attr(angle a)
{
    set_attr(a.value(), "angle");
}

void xml_elem::set_attr(bool b, const std::string& name)
{
    set_attr(unsigned(b), name);
}

auto xml_elem::get_name() const -> const std::string&
{
    return elem->ValueStr();
}

void xml_elem::add_child_text(const std::string& txt)
{
    elem->LinkEndChild(new TiXmlText(txt));
}

auto xml_elem::child_text() const -> const std::string&
{
    auto* ntext = elem->FirstChild();
    if (ntext == nullptr) {
        THROW(xml_error, std::string("child of ") + get_name() + std::string(" is no text node"), doc_name());
    }
    return ntext->ValueStr();
}

auto xml_elem::begin() const -> xml_elem::iterator
{
    return iterator(*this, elem->FirstChildElement(), false);
}

auto xml_elem::iterator_range_samename::begin() const -> xml_elem::iterator
{
    return iterator(parent, parent.elem->FirstChildElement(childname), true);
}

auto xml_elem::iterator::operator*() const -> xml_elem
{
    if (e == nullptr) {
        THROW(xml_error, "elem() on empty iterator", parent.doc_name());
    }
    return {e};
}

auto xml_elem::iterator::operator++() -> xml_elem::iterator&
{
    if (e == nullptr) {
        THROW(xml_error, "next() on empty iterator", parent.doc_name());
    }
    if (samename) {
        e = e->NextSiblingElement(e->ValueStr());
    } else {
        e = e->NextSiblingElement();
    }
    return *this;
}

xml_doc::xml_doc(const std::string& fn)
    : doc(new TiXmlDocument(fn))
{
}

xml_doc::~xml_doc()
{
    // needed to make unique_ptr compile
}

void xml_doc::load()
{
    if (!doc->LoadFile()) {
        THROW(xml_error, std::string("can't load: ") + doc->ErrorDesc(), doc->ValueStr());
    }
}

void xml_doc::save()
{
    if (!doc->SaveFile()) {
        THROW(xml_error, std::string("can't save: ") + doc->ErrorDesc(), doc->ValueStr());
    }
}

auto xml_doc::first_child() -> xml_elem
{
    auto* e = doc->FirstChildElement();
    if (e == nullptr) {
        THROW(xml_elem_error, "<first-child>", doc->ValueStr());
    }
    return {e};
}

auto xml_doc::child(const std::string& name) -> xml_elem
{
    auto* e = doc->FirstChildElement(name);
    if (e == nullptr) {
        THROW(xml_elem_error, name, doc->ValueStr());
    }
    return {e};
}

auto xml_doc::add_child(const std::string& name) -> xml_elem
{
    auto* e = new TiXmlElement(name);
    doc->LinkEndChild(e);
    return {e};
}

auto xml_doc::get_filename() const -> const std::string&
{
    return doc->ValueStr();
}
