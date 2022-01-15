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

#include "Layout.h"
#include "System.h"
#include "../lua/Layout.h"
#include <iostream>
#include <lualib.h>
#include <lauxlib.h>
using namespace std;
namespace layout
{
Layout::Layout(const char *name, const char *label, int mask, lua::Ref &&callback):
	m_name(name),
	m_label(label),
	m_mask(mask),
	m_callback(move(callback))
{
}
const std::string &Layout::name() const
{
	return m_name;
}
const std::string &Layout::label() const
{
	return m_label;
}
const int Layout::mask() const
{
	return m_mask;
}
System *Layout::build()
{
	lua_State *L = m_callback.script();
	m_callback.get();
	System *system = new System();
	lua::pushLayoutSystem(L, system);
	int status;
	if ((status = lua_pcall(L, 1, 0, 0)) == 0){
		return system;
	}else{
		cerr << "layout.build: " << lua_tostring(L, -1) << endl;
	}
	lua_pop(L, 1);
	delete system;
	return nullptr;
}
}
