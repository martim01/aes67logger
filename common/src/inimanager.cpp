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
#include "inimanager.h"
#include "inisection.h"
#include <sstream>
#include "log.h"


iniManager::iniManager()
{
}


iniManager::~iniManager()
{

	DeleteSections();
}

void iniManager::DeleteSections()
{
	m_mSections.clear();
}

bool iniManager::ReRead()
{
    if(m_filepath.empty())
    {
        return false;
    }
    return Read(m_filepath);
}

/*!
    \fn iniManager::ReadIniFile(const string& sFilename)
 */
bool iniManager::Read(const std::filesystem::path& filepath)
{
    m_filepath = filepath;

	m_mSections.clear();

    std::ifstream ifFile;

    //attempt to open the file
    ifFile.open(m_filepath,std::ios::in);
    if(!ifFile.is_open())
    {
        return false;
    }

    std::stringstream isstr;
    isstr << ifFile.rdbuf();
    ifFile.close();


    std::string sLine;
    std::string sTag;
    std::string sData;

    unsigned long nLine(0);
    //read in each line
    auto itSection = m_mSections.end();

    while(getline(isstr, sLine))
    {
        ++nLine;
        // if the line starts with a [ then its the start of a section
        if(sLine[0] == '[')
        {
            std::string sSection;
            sSection.reserve(sLine.length());

            for(size_t i = 1; i < sLine.length(); i++)
            {
                if(sLine[i] == ']')
                    break;
                sSection+=sLine[i];
            }
            //get the name of the section
            itSection = m_mSections.insert(std::make_pair(sSection, std::make_shared<iniSection>(sSection))).first;
        }
        else if(itSection != m_mSections.end() && sLine[0] != '#' && sLine[0] != ';' && sLine.size() >= 2)
        {
            size_t nEqualPos = std::string::npos;
            size_t nCommentPos = 0;
            std::string sKey;
            std::string sData;
            sKey.reserve(sLine.length());
            sData.reserve(sLine.length());

            for(;nCommentPos < sLine.length(); nCommentPos++)
            {
                if(sLine[nCommentPos] == '#' || sLine[nCommentPos] == ';')
                {
                    break;
                }
                else if(sLine[nCommentPos] == '=' && nEqualPos == std::string::npos)
                {
                    nEqualPos = nCommentPos;
                }
                else if(nEqualPos == std::string::npos)
                {
                    sKey += sLine[nCommentPos];
                }
                else
                {
                    sData += sLine[nCommentPos];
                }
            }
            if(nEqualPos != std::string::npos)
            {
                itSection->second->Set(sKey, sData);
            }
        }
    }

    return true;
}


std::shared_ptr<iniSection> iniManager::GetSection(const std::string& sSectionName) const
{
    auto it = m_mSections.find(sSectionName);
    if(it == m_mSections.end())
        return nullptr;
    return it->second;
}

/*!
    \fn iniManager::GetIniString(const string& sSection, const string& sKey, const string& sDefault)
 */
const std::string& iniManager::Get(const std::string& sSection, const std::string& sKey, const std::string& sDefault) const
{
    //does the section exist?
    auto it = m_mSections.find(sSection);
	if(it==m_mSections.end())
		return sDefault;


	return it->second->Get(sKey,sDefault);
}

/*!
    \fn iniManager::GetIniInt(const string& sSection, const string& sKey, int nDefault)
 */
int iniManager::Get(const std::string& sSection, const std::string& sKey, int nDefault) const
{
    //does the section exist?
	auto it = m_mSections.find(sSection);
	if(it==m_mSections.end())
		return nDefault;

	return it->second->Get(sKey,nDefault);
}

/*!
    \fn iniManager::GetIniDouble(const string& sSection, const string& sKey, double dDefault)
 */
double iniManager::Get(const std::string& sSection, const std::string& sKey, double dDefault) const
{
    //does the section exist?
	auto it = m_mSections.find(sSection);
	if(it==m_mSections.end())
		return dDefault;

	return it->second->Get(sKey,dDefault);
}

bool iniManager::Write()
{
    return Write(m_filepath);
}

bool iniManager::Write(const std::filesystem::path& filepath)
{
	m_filepath = filepath;

    //Close file if open
	if(m_of.is_open())
    {
        m_of.close();
	}

	//attempt to open the file

	m_of.open(m_filepath);
	if(!m_of.is_open())
	{
		return false;
	}

	m_of.clear();

	for(const auto& pairSection : m_mSections)
    {
        pairSection.second->Write(m_of);
    }
    //Close the file again
	if(m_of.is_open())
	{
	    m_of.close();
	}

    return true;

}

std::shared_ptr<iniSection> iniManager::CreateSection(const std::string& sSectionName)
{
    auto itSection =m_mSections.find(sSectionName);
    if(itSection == m_mSections.end())
    {
        itSection  = m_mSections.insert(make_pair(sSectionName,std::make_shared<iniSection>(sSectionName))).first;
    }
    return itSection->second;
}

void iniManager::Set(const std::string& sSectionName, const std::string& sKey, const std::string& sValue)
{
    auto pSection = CreateSection(sSectionName);
    pSection->Set(sKey,sValue);
}

void iniManager::Set(const std::string& sSectionName, const std::string& sKey, int nValue)
{
    std::stringstream ss;
    ss << nValue;
    Set(sSectionName, sKey, ss.str());
}

void iniManager::Set(const std::string& sSectionName, const std::string& sKey, double dValue)
{
    std::stringstream ss;
    ss << dValue;
    Set(sSectionName, sKey, ss.str());
}

size_t iniManager::GetNumberOfSectionEntries(const std::string& sSectionName) const
{
	auto it = m_mSections.find(sSectionName);
	if(it== m_mSections.end())
        return 0;
    return it->second->GetNumberOfEntries();
}

bool iniManager::DeleteSection(const std::string& sSectionName)
{
    auto pSection = GetSection(sSectionName);
    if(pSection)
    {
        m_mSections.erase(sSectionName);
        return true;
    }
    return false;
}
