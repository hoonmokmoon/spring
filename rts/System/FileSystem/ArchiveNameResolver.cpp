/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "ArchiveNameResolver.h"

#include "Game/GlobalUnsynced.h"
#include "System/FileSystem/ArchiveScanner.h"
#include "System/FileSystem/RapidHandler.h"
#include "System/UriParser.h"
#include "System/Util.h"

#include <iostream>
#include <boost/cstdint.hpp>

namespace ArchiveNameResolver {
	//////////////////////////////////////////////////////////////////////////////
//
//  Helpers
//

	static boost::uint64_t ExtractVersionNumber(const std::string& version)
	{
		std::istringstream iss(version);
		boost::uint64_t versionInt = 0;
		int num;
		while (true) {
			if (iss >> num) {
				versionInt = versionInt * 1000 + std::abs(num);
			} else
			if (iss.eof()) {
				break;
			} else
			if (iss.fail()) {
				iss.clear();
				iss.ignore();
			}
		}
		return versionInt;
	}


	static bool GetGameByExactName(const std::string& lazyName, std::string* applicableName)
	{
		const CArchiveScanner::ArchiveData& aData = archiveScanner->GetArchiveData(lazyName);

		std::string error;
		if (aData.IsValid(error)) {
			if (aData.GetModType() == modtype::primary) {
				*applicableName = lazyName;
				return true;
			}
		}

		return false;
	}


	static bool GetGameByShortName(const std::string& lazyName, std::string* applicableName)
	{
		const std::string lowerLazyName = StringToLower(lazyName);
		const std::vector<CArchiveScanner::ArchiveData>& found = archiveScanner->GetPrimaryMods();

		std::string matchingName;
		std::string matchingVersion;
		boost::uint64_t matchingVersionInt = 0;

		for (std::vector<CArchiveScanner::ArchiveData>::const_iterator it = found.begin(); it != found.end(); ++it) {
			if (lowerLazyName == StringToLower(it->GetShortName())) {
				// find latest version of the game
				boost::uint64_t versionInt = ExtractVersionNumber(it->GetVersion());

				if (versionInt > matchingVersionInt) {
					matchingName = it->GetNameVersioned();
					matchingVersion = it->GetVersion();
					matchingVersionInt = versionInt;
					continue;
				}

				if (versionInt == matchingVersionInt) {
					// very bad solution, fails with `10.0` vs. `9.10`
					const int compareInt = matchingVersion.compare(it->GetVersion());
					if (compareInt <= 0) {
						matchingName = it->GetNameVersioned();
						matchingVersion = it->GetVersion();
						//matchingVersionInt = versionInt;
					}
				}
			}
		}

		if (!matchingName.empty()) {
			*applicableName = matchingName;
			return true;
		}

		return false;
	}


	static bool GetRandomGame(const std::string& lazyName, std::string* applicableName)
	{
#ifndef UNITSYNC
		if (std::string("random").find(lazyName) != std::string::npos) {
			const std::vector<CArchiveScanner::ArchiveData>& games = archiveScanner->GetPrimaryMods();
			if (!games.empty()) {
				*applicableName = games[gu->RandInt() % games.size()].GetNameVersioned();
				return true;
			}
		}
#endif //UNITSYNC
		return false;
	}


	static bool GetMapByExactName(const std::string& lazyName, std::string* applicableName)
	{
		const CArchiveScanner::ArchiveData& aData = archiveScanner->GetArchiveData(lazyName);

		std::string error;
		if (aData.IsValid(error)) {
			if (aData.GetModType() == modtype::map) {
				*applicableName = lazyName;
				return true;
			}
		}

		return false;
	}


	static bool GetMapBySubString(const std::string& lazyName, std::string* applicableName)
	{
		const std::string lowerLazyName = StringToLower(lazyName);
		const std::vector<std::string>& found = archiveScanner->GetMaps();

		std::vector<std::string> substrings;
		std::istringstream iss(lowerLazyName);
		std::string buf;
		while (iss >> buf) {
			substrings.push_back(buf);
		}

		std::string matchingName;
		size_t matchingLength = 1e6;

		for (std::vector<std::string>::const_iterator it = found.begin(); it != found.end(); ++it) {
			const std::string lowerMapName = StringToLower(*it);

			// search for all wanted substrings
			bool fits = true;
			for (std::vector<std::string>::const_iterator jt = substrings.begin(); jt != substrings.end(); ++jt) {
				const std::string& substr = *jt;
				if (lowerMapName.find(substr) == std::string::npos) {
					fits = false;
					break;
				}
			}

			if (fits) {
				// shortest fitting string wins
				const int nameLength = lowerMapName.length();
				if (nameLength < matchingLength) {
					matchingName = *it;
					matchingLength = nameLength;
				}
			}
		}

		if (!matchingName.empty()) {
			*applicableName = matchingName;
			return true;
		}

		return false;
	}


	static bool GetRandomMap(const std::string& lazyName, std::string* applicableName)
	{
#ifndef UNITSYNC
		if (std::string("random").find(lazyName) != std::string::npos) {
			const std::vector<std::string>& maps = archiveScanner->GetMaps();
			if (!maps.empty()) {
				*applicableName = maps[gu->RandInt() % maps.size()];
				return true;
			}
		}
#endif //UNITSYNC
		return false;
	}

	static bool GetGameByRapidTag(const std::string& lazyName, std::string& tag)
	{
		if (!ParseRapidUri(lazyName, tag))
			return false;
		tag = GetRapidName(tag);
		return !tag.empty();
	}

//////////////////////////////////////////////////////////////////////////////
//
//  Interface
//

std::string GetGame(const std::string& lazyName)
{
	std::string applicableName = lazyName;
	if (GetGameByExactName(lazyName, &applicableName)) return applicableName;
	if (GetGameByShortName(lazyName, &applicableName)) return applicableName;
	if (GetRandomGame(lazyName, &applicableName))      return applicableName;
	if (GetGameByRapidTag(lazyName, applicableName))   return applicableName;

	return lazyName;
}

std::string GetMap(const std::string& lazyName)
{
	std::string applicableName = lazyName;
	if (GetMapByExactName(lazyName, &applicableName)) return applicableName;
	if (GetMapBySubString(lazyName, &applicableName)) return applicableName;
	if (GetRandomMap(lazyName, &applicableName))      return applicableName;
	//TODO add a string similarity search?

	return lazyName;
}

} //namespace ArchiveNameResolver
