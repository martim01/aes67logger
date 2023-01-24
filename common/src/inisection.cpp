/***************************************************************************
 *   Copyright (C) 2005 by Matthew Martin   *
 *   matthew@nabiezzi.plus.com   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#include "inisection.h"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <algorithm>

#include "log.h"

iniSection::iniSection(const std::string& sSection)
: m_sSectionName(sSection)
{

}


iniSection::~iniSection()
{
}


/*!
    \fn iniSection::GetDataBegin()
 */
mapIniData::const_iterator iniSection::GetDataBegin() const
{
    return m_mSectionData.begin();
}

/*!
    \fn iniSection::GetDataEnd()
 */
mapIniData::const_iterator iniSection::GetDataEnd() const
{
    return m_mSectionData.end();
}

const mapIniData& iniSection::GetData() const
{
    return m_mSectionData;
}


/*!
    \fn iniSection::GetString(const std::string& sKey, const std::string& sDefault)
 */
const std::string& iniSection::Get(const std::string& sKey, const std::string& sDefault) const
{
  	//does the key exist
  	auto it = m_mSectionData.find(sKey);
	if(it==m_mSectionData.end())
		return sDefault;
	return it->second;
}

/*!
    \fn iniSection::GetInt(const std::string& sKey, int nDefault)
 */
int iniSection::Get(const std::string& sKey, int nDefault)
{
  	//does the key exist
  	auto it = m_mSectionData.find(sKey);
	if(it==m_mSectionData.end())
		return nDefault;
    long n = nDefault;

    n = atoi(it->second.c_str());
	return n;
}

/*!
    \fn iniSection::Getstd::string(const std::string& sKey, double dDefault)
 */
double iniSection::Get(const std::string& sKey, double dDefault)
{
  	//does the key exist
	auto it = m_mSectionData.find(sKey);
	if(it==m_mSectionData.end())
		return dDefault;
    double d = dDefault;
    d = atof(it->second.c_str());
    return d;
}

bool iniSection::Get(const std::string& sKey, bool bDefault)
{
  	//does the key exist
	auto it = m_mSectionData.find(sKey);
	if(it==m_mSectionData.end())
		return bDefault;

    return (it->second == "1" || it->second == "true" || it->second == "TRUE");
}


void iniSection::Set(const std::string& sKey, const std::string& sValue)
{
	m_mSectionData[sKey] = sValue;
}

size_t iniSection::GetNumberOfEntries()
{
    return m_mSectionData.size();
}

mapIniData::const_iterator iniSection::FindData(const std::string& sKey) const
{
    return m_mSectionData.find(sKey);
}

void iniSection::Write(std::ofstream& of)
{
    //write the section name
    of << "[" <<  m_sSectionName << "]\n";

    //now write the data
    for(const auto& pairData : m_mSectionData)
    {
        of << pairData.first << "=" << pairData.second << "\n";
    }
}
