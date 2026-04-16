/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-License-Identifier: GPL-3.0-or-later WITH LicenseRef-MeshMC-MMCO-Module-Exception-1.0
 *
 *   MeshMC - A Custom Launcher for Minecraft
 *   Copyright (C) 2026 Project Tick
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version, with the additional permission
 *   described in the MeshMC MMCO Module Exception 1.0.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 *   You should have received a copy of the MeshMC MMCO Module Exception 1.0
 *   along with this program.  If not, see <https://projecttick.org/licenses/>.
 */

#include "classfile.h"
#include "annotations.h"
#include <sstream>

namespace java
{
	std::string annotation::toString()
	{
		std::ostringstream ss;
		ss << "Annotation type : " << type_index << " - "
		   << pool[type_index].str_data << std::endl;
		ss << "Contains " << name_val_pairs.size() << " pairs:" << std::endl;
		for (unsigned i = 0; i < name_val_pairs.size(); i++) {
			std::pair<uint16_t, element_value*>& val = name_val_pairs[i];
			auto name_idx = val.first;
			ss << pool[name_idx].str_data << "(" << name_idx << ")"
			   << " = " << val.second->toString() << std::endl;
		}
		return ss.str();
	}

	annotation* annotation::read(util::membuffer& input, constant_pool& pool)
	{
		uint16_t type_index = 0;
		input.read_be(type_index);
		annotation* ann = new annotation(type_index, pool);

		uint16_t num_pairs = 0;
		input.read_be(num_pairs);
		while (num_pairs) {
			uint16_t name_idx = 0;
			// read name index
			input.read_be(name_idx);
			auto elem = element_value::readElementValue(input, pool);
			// read value
			ann->add_pair(name_idx, elem);
			num_pairs--;
		}
		return ann;
	}

	element_value* element_value::readElementValue(util::membuffer& input,
												   java::constant_pool& pool)
	{
		element_value_type type = INVALID;
		input.read(type);
		uint16_t index = 0;
		uint16_t index2 = 0;
		std::vector<element_value*> vals;
		switch (type) {
			case PRIMITIVE_BYTE:
			case PRIMITIVE_CHAR:
			case PRIMITIVE_DOUBLE:
			case PRIMITIVE_FLOAT:
			case PRIMITIVE_INT:
			case PRIMITIVE_LONG:
			case PRIMITIVE_SHORT:
			case PRIMITIVE_BOOLEAN:
			case STRING:
				input.read_be(index);
				return new element_value_simple(type, index, pool);
			case ENUM_CONSTANT:
				input.read_be(index);
				input.read_be(index2);
				return new element_value_enum(type, index, index2, pool);
			case CLASS: // Class
				input.read_be(index);
				return new element_value_class(type, index, pool);
			case ANNOTATION: // Annotation
				// FIXME: runtime visibility info needs to be passed from parent
				return new element_value_annotation(
					ANNOTATION, annotation::read(input, pool), pool);
			case ARRAY: // Array
				input.read_be(index);
				for (int i = 0; i < index; i++) {
					vals.push_back(
						element_value::readElementValue(input, pool));
				}
				return new element_value_array(ARRAY, vals, pool);
			default:
				throw new java::classfile_exception();
		}
	}
} // namespace java