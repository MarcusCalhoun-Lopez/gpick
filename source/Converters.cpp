/*
 * Copyright (c) 2009-2017, Albertas Vyšniauskas
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 *     * Neither the name of the software author nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "Converters.h"
#include "Converter.h"
#include "ColorObject.h"
#include "common/First.h"
#include <unordered_set>
Converters::Converters() {
}
Converters::~Converters() {
	for (auto converter: m_allConverters) {
		delete converter;
	}
}
void Converters::add(Converter *converter) {
	if (m_converters.find(converter->name()) != m_converters.end()) {
		delete converter;
		return;
	}
	m_allConverters.push_back(converter);
	if (converter->copy() && converter->hasSerialize())
		m_copyConverters.push_back(converter);
	if (converter->paste() && converter->hasDeserialize())
		m_pasteConverters.push_back(converter);
	m_converters[converter->name()] = converter;
}
void Converters::add(const char *name, const char *label, Converter::Callback<Converter::Serialize> serialize, Converter::Callback<Converter::Deserialize> deserialize) {
	add(new Converter(name, label, serialize, deserialize));
}
void Converters::add(const char *name, const std::string &label, Converter::Callback<Converter::Serialize> serialize, Converter::Callback<Converter::Deserialize> deserialize) {
	add(new Converter(name, label.c_str(), serialize, deserialize));
}
void Converters::rebuildCopyPasteArrays() {
	m_copyConverters.clear();
	m_pasteConverters.clear();
	for (auto converter: m_allConverters) {
		if (converter->copy() && converter->hasSerialize())
			m_copyConverters.push_back(converter);
		if (converter->paste() && converter->hasDeserialize())
			m_pasteConverters.push_back(converter);
	}
}
const std::vector<Converter *> &Converters::all() const {
	return m_allConverters;
}
const std::vector<Converter *> &Converters::allCopy() const {
	return m_copyConverters;
}
bool Converters::hasCopy() const {
	return m_copyConverters.size() != 0;
}
const std::vector<Converter *> &Converters::allPaste() const {
	return m_pasteConverters;
}
Converter *Converters::byName(const char *name) const {
	auto i = m_converters.find(name);
	if (i != m_converters.end())
		return i->second;
	return nullptr;
}
Converter *Converters::byName(const std::string &name) const {
	auto i = m_converters.find(name);
	if (i != m_converters.end())
		return i->second;
	return nullptr;
}
Converter *Converters::display() const {
	return m_displayConverter;
}
Converter *Converters::colorList() const {
	return m_colorListConverter;
}
Converter *Converters::forType(Type type) const {
	switch (type) {
	case Type::display:
		return m_displayConverter;
	case Type::colorList:
		return m_colorListConverter;
	case Type::copy:
		return m_copyConverters.size() != 0 ? m_copyConverters.front() : nullptr;
	}
	return nullptr;
}
void Converters::display(const char *name) {
	m_displayConverter = byName(name);
}
void Converters::colorList(const char *name) {
	m_colorListConverter = byName(name);
}
void Converters::display(const std::string &name) {
	m_displayConverter = byName(name);
}
void Converters::colorList(const std::string &name) {
	m_colorListConverter = byName(name);
}
void Converters::display(Converter *converter) {
	m_displayConverter = converter;
}
void Converters::colorList(Converter *converter) {
	m_colorListConverter = converter;
}
Converter *Converters::firstCopy() const {
	if (m_copyConverters.size() == 0)
		return nullptr;
	return m_copyConverters.front();
}
Converter *Converters::firstCopyOrAny() const {
	if (m_copyConverters.size() == 0) {
		if (m_allConverters.size() == 0)
			return nullptr;
		return m_allConverters.front();
	}
	return m_copyConverters.front();
}
Converter *Converters::byNameOrFirstCopy(const char *name) const {
	if (name) {
		Converter *result = byName(name);
		if (result)
			return result;
	}
	if (m_copyConverters.size() == 0)
		return nullptr;
	return m_copyConverters.front();
}
std::string Converters::serialize(const ColorObject &colorObject, Type type) {
	Converter *converter;
	switch (type) {
	case Type::colorList:
		converter = colorList();
		break;
	case Type::display:
		converter = display();
		break;
	default:
		converter = nullptr;
	}
	if (converter)
		return converter->serialize(colorObject);
	converter = firstCopyOrAny();
	if (converter)
		return converter->serialize(colorObject);
	return "";
}
std::string Converters::serialize(const Color &color, Type type) {
	ColorObject colorObject("", color);
	return serialize(colorObject, type);
}
bool Converters::deserialize(const std::string &value, ColorObject &outputColorObject) {
	common::First<float, std::greater<float>, ColorObject> bestConversion;
	ColorObject colorObject;
	float quality;
	if (m_displayConverter) {
		Converter *converter = m_displayConverter;
		if (converter->hasDeserialize()) {
			if (converter->deserialize(value.c_str(), colorObject, quality)) {
				if (quality > 0) {
					bestConversion(quality, colorObject);
				}
			}
		}
	}
	for (auto &converter: m_pasteConverters) {
		if (converter == m_displayConverter || !converter->hasDeserialize())
			continue;
		if (converter->deserialize(value.c_str(), colorObject, quality)) {
			if (quality > 0) {
				bestConversion(quality, colorObject);
			}
		}
	}
	if (!bestConversion)
		return false;
	outputColorObject = bestConversion.data<ColorObject>();
	return true;
}
void Converters::reorder(const char **names, size_t count) {
	std::unordered_set<Converter *> used;
	std::vector<Converter *> converters;
	for (size_t i = 0; i < count; i++) {
		auto converter = byName(names[i]);
		if (converter) {
			used.insert(converter);
			converters.push_back(converter);
		}
	}
	for (auto converter: m_allConverters) {
		if (used.count(converter) == 0) {
			converters.push_back(converter);
		}
	}
	m_allConverters.clear();
	m_allConverters = converters;
}
void Converters::reorder(const std::vector<std::string> &names) {
	std::unordered_set<Converter *> used;
	std::vector<Converter *> converters;
	for (size_t i = 0; i < names.size(); i++) {
		auto converter = byName(names[i]);
		if (converter) {
			used.insert(converter);
			converters.push_back(converter);
		}
	}
	for (auto converter: m_allConverters) {
		if (used.count(converter) == 0) {
			converters.push_back(converter);
		}
	}
	m_allConverters.clear();
	m_allConverters = converters;
}
