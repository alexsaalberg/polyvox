/******************************************************************************
This file is part of a voxel plugin for OGRE
Copyright (C) 2006  David Williams

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
******************************************************************************/
#ifndef __IntegralVector3_H__
#define __IntegralVector3_H__

#include <OgrePrerequisites.h>

namespace Ogre
{
	template <typename Type> class IntegralVector3
	{
	public:
		IntegralVector3(Type xToSet, Type yToSet, Type zToSet)
		{
			x = xToSet;
			y = yToSet;
			z = zToSet;
		}

		bool operator==(const IntegralVector3<Type>& rhs) const throw()
		{
			return ((x == rhs.x) && (y == rhs.y) && (z == rhs.z));
		}

		bool operator<(const IntegralVector3<Type>& rhs) const throw()
		{
			if(x != rhs.x)
				return (x < rhs.x);
			else if(y != rhs.y)
				return (y < rhs.y);
			else if(z != rhs.z)
				return (z < rhs.z);
			else
				return false; //They are equal
		}

		Type x;
		Type y;
		Type z;
	};

	typedef IntegralVector3<char> CharVector3;
	typedef IntegralVector3<short> ShortVector3;
	typedef IntegralVector3<int> IntVector3;
	typedef IntegralVector3<long> LongVector3;

	typedef IntegralVector3<uchar> UCharVector3;
	typedef IntegralVector3<ushort> UShortVector3;
	typedef IntegralVector3<uint> UIntVector3;
	typedef IntegralVector3<ulong> ULongVector3;
}

#endif