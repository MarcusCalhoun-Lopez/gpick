/*
 * Copyright (c) 2009-2020, Albertas Vyšniauskas
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

#include <boost/test/unit_test.hpp>
#include <fstream>
#include <iostream>
#include <vector>
#include "parser/TextFile.h"
#include "Color.h"
using namespace text_file_parser;

namespace {
struct Parser: public TextFile {
	std::istream *m_stream;
	std::vector<Color> m_colors;
	bool m_failed;
	Parser(std::istream *stream) {
		m_stream = stream;
		m_failed = false;
	}
	virtual ~Parser() {
	}
	virtual void outOfMemory() {
		m_failed = true;
	}
	virtual void syntaxError(size_t startLine, size_t startColumn, size_t endLine, size_t endEolunn) {
		m_failed = true;
	}
	virtual size_t read(char *buffer, size_t length) {
		m_stream->read(buffer, length);
		size_t bytes = m_stream->gcount();
		if (bytes > 0) return bytes;
		if (m_stream->eof()) return 0;
		if (!m_stream->good()) {
			m_failed = true;
		}
		return 0;
	}
	virtual void addColor(const Color &color) {
		m_colors.push_back(color);
	}
	size_t count() {
		return m_colors.size();
	}
	bool checkColor(size_t index, const Color &color) {
		return m_colors[index] == color;
	}
	void parse() {
		Configuration configuration;
		TextFile::parse(configuration);
	}
};
}
BOOST_AUTO_TEST_SUITE(textFileParser)
BOOST_AUTO_TEST_CASE(fullHex) {
	std::ifstream file("test/textImport01.txt");
	BOOST_REQUIRE(file.is_open());
	Parser parser(&file);
	parser.parse();
	BOOST_CHECK(parser.count() == 1);
	Color color = { 0xaa, 0xbb, 0xcc };
	BOOST_CHECK(parser.checkColor(0, color));
	file.close();
}
BOOST_AUTO_TEST_CASE(shortHex) {
	std::ifstream file("test/textImport02.txt");
	BOOST_REQUIRE(file.is_open());
	Parser parser(&file);
	parser.parse();
	BOOST_CHECK(parser.count() == 1);
	Color color = { 0xaa, 0xbb, 0xcc };
	BOOST_CHECK(parser.checkColor(0, color));
	file.close();
}
BOOST_AUTO_TEST_CASE(cssRgb) {
	std::ifstream file("test/textImport03.txt");
	BOOST_REQUIRE(file.is_open());
	Parser parser(&file);
	parser.parse();
	BOOST_CHECK(parser.count() == 1);
	Color color = { 0xaa, 0xbb, 0xcc };
	BOOST_CHECK(parser.checkColor(0, color));
	file.close();
}
BOOST_AUTO_TEST_CASE(cssRgba) {
	std::ifstream file("test/textImport04.txt");
	BOOST_REQUIRE(file.is_open());
	Parser parser(&file);
	parser.parse();
	BOOST_CHECK(parser.count() == 1);
	Color color = { 0xaa, 0xbb, 0xcc };
	color[3] = 0.5f;
	BOOST_CHECK(parser.checkColor(0, color));
	file.close();
}
BOOST_AUTO_TEST_CASE(intValues) {
	std::ifstream file("test/textImport05.txt");
	BOOST_REQUIRE(file.is_open());
	Parser parser(&file);
	parser.parse();
	BOOST_CHECK(parser.count() == 1);
	Color color = { 0xaa, 0xbb, 0xcc };
	BOOST_CHECK(parser.checkColor(0, color));
	file.close();
}
BOOST_AUTO_TEST_CASE(floatValuesSeparatedByComma) {
	std::ifstream file("test/textImport06.txt");
	BOOST_REQUIRE(file.is_open());
	Parser parser(&file);
	parser.parse();
	BOOST_CHECK(parser.count() == 1);
	Color color = { 0xaa, 0xbb, 0xcc };
	BOOST_CHECK(parser.checkColor(0, color));
	file.close();
}
BOOST_AUTO_TEST_CASE(floatValuesSeparatedBySpace) {
	std::ifstream file("test/textImport10.txt");
	BOOST_REQUIRE(file.is_open());
	Parser parser(&file);
	parser.parse();
	BOOST_CHECK(parser.count() == 1);
	Color color = { 0xaa, 0xbb, 0xcc };
	BOOST_CHECK(parser.checkColor(0, color));
	file.close();
}
BOOST_AUTO_TEST_CASE(singleLineCComments) {
	std::ifstream file("test/textImport07.txt");
	BOOST_REQUIRE(file.is_open());
	Parser parser(&file);
	parser.parse();
	BOOST_CHECK(parser.count() == 1);
	Color color = { 0xaa, 0xbb, 0xcc };
	BOOST_CHECK(parser.checkColor(0, color));
	file.close();
}
BOOST_AUTO_TEST_CASE(multiLineCComments) {
	std::ifstream file("test/textImport08.txt");
	BOOST_REQUIRE(file.is_open());
	Parser parser(&file);
	parser.parse();
	BOOST_CHECK(parser.count() == 1);
	Color color = { 0xaa, 0xbb, 0xcc };
	BOOST_CHECK(parser.checkColor(0, color));
	file.close();
}
BOOST_AUTO_TEST_CASE(singleLineHashComments) {
	std::ifstream file("test/textImport09.txt");
	BOOST_REQUIRE(file.is_open());
	Parser parser(&file);
	parser.parse();
	BOOST_CHECK(parser.count() == 1);
	Color color = { 0xaa, 0xbb, 0xcc };
	BOOST_CHECK(parser.checkColor(0, color));
	file.close();
}
BOOST_AUTO_TEST_CASE(outOfRangeFloatValue) {
	std::ifstream file("test/textImport11.txt");
	BOOST_REQUIRE(file.is_open());
	Parser parser(&file);
	parser.parse();
	BOOST_CHECK(parser.count() == 1);
	Color color = { 0xaa, 0xbb, 0 };
	BOOST_CHECK(parser.checkColor(0, color));
	file.close();
}
BOOST_AUTO_TEST_SUITE_END()
