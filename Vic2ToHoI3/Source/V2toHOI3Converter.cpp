/*Copyright (c) 2015 The Paradox Game Converters Project

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.*/



#include <fstream>
#include <io.h>
#include <stdexcept>
#include <sys/stat.h>

#include <Windows.h>

#include "Configuration.h"
#include "Log.h"
#include "Parsers\Parser.h"
#include "HOI3World\HoI3World.h"
#include "V2World\V2World.h"
#include "V2World\V2Factory.h"
#include "V2World\V2Localisation.h"
#include "WinUtils.h"


// Converts the given V2 save into a HOI3 mod.
// Returns 0 on success or a non-zero failure code on error.
int ConvertV2ToHoI3(const std::string& V2SaveFileName)
{
	Object*	obj;					// generic object

	Configuration::getInstance();

	char curDir[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, curDir);
	LOG(LogLevel::Debug) << "Current directory is " << curDir;

	// Get HoI3 install location
	LOG(LogLevel::Debug) << "Get HoI3 Install Path";
	string HoI3Loc = Configuration::getHoI3Path();	// the HOI3 install location as stated in the configuration file
	struct _stat st;										// the file info
	if (HoI3Loc.empty() || (_stat(HoI3Loc.c_str(), &st) != 0))
	{
		LOG(LogLevel::Error) << "No HoI3 path was specified in configuration.txt, or the path was invalid";
		return (-1);
	}
	else
	{
		LOG(LogLevel::Debug) << "HoI3 path install path is " << HoI3Loc;
	}

	// Get HOI3 Documents Directory
	LOG(LogLevel::Debug) << "Get HOI3 Documents directory";
	string HoI3DocLoc = Configuration::getHoI3DocumentsPath();	// the HoI3 My Documents location as stated in the configuration file
	if (HoI3DocLoc.empty() || (_stat(HoI3DocLoc.c_str(), &st) != 0))
	{
		LOG(LogLevel::Error) << "No HoI3 documents directory was specified in configuration.txt, or the path was invalid";
		return (-1);
	}
	else
	{
		LOG(LogLevel::Debug) << "HoI3 documents directory is " << HoI3DocLoc;
	}

	// Get V2 install location
	LOG(LogLevel::Debug) << "Get V2 Install Path";
	string V2Loc = Configuration::getV2Path();	// the V2 install location as stated in the configuration file
	if (V2Loc.empty() || (_stat(V2Loc.c_str(), &st) != 0))
	{
		LOG(LogLevel::Error) << "No Victoria 2 path was specified in configuration.txt, or the path was invalid";
		return (-1);
	}
	else
	{
		LOG(LogLevel::Debug) << "Victoria 2 install path is " << V2Loc;
	}

	// Get V2 Mod directory
	map<string, string> possibleMods; // name, path
	LOG(LogLevel::Debug) << "Get V2 Documents Directory";
	string V2DocumentsLoc = Configuration::getV2DocumentsPath();	// the Victoria 2 My Documents location as stated in the configuration file
	if (V2DocumentsLoc.empty() || (_stat(V2DocumentsLoc.c_str(), &st) != 0))
	{
		LOG(LogLevel::Error) << "No Victoria 2 documents directory was specified in configuration.txt, or the path was invalid";
		return (-1);
	}
	else
	{
		LOG(LogLevel::Debug) << "Victoria 2 Documents directory is " << V2DocumentsLoc;
	}

	// Sanity check Vic2 mods
	LOG(LogLevel::Debug) << "Double-check Vic2 mods";
	vector<string> vic2Mods = Configuration::getVic2Mods();
	for (auto itr: vic2Mods)
	{
		LOG(LogLevel::Debug) << "\tExpecting a mod with name " << itr;
	}

	set<string> fileNames;
	WinUtils::GetAllFilesInFolder(Configuration::getV2Path() + "\\mod", fileNames);
	for (set<string>::iterator itr = fileNames.begin(); itr != fileNames.end(); itr++)
	{
		const int pos = itr->find_last_of('.');	// the position of the last period in the filename
		if (itr->substr(pos, itr->length()) == ".mod")
		{
			string folderName = itr->substr(0, pos);
			if (WinUtils::doesFolderExist(Configuration::getV2Path() + "\\mod\\" + folderName))
			{
				LOG(LogLevel::Debug) << "\tFound mod with name " << folderName;
			}
		}
	}

	//get output name
	const int slash	= V2SaveFileName.find_last_of("\\");				// the last slash in the save's filename
	string outputName = V2SaveFileName.substr(slash + 1, V2SaveFileName.length());
	const int length	= outputName.find_first_of(".");						// the first period after the slash
	outputName			= outputName.substr(0, length);						// the name to use to output the mod
	int dash = outputName.find_first_of('-');									// the first (if any) dask in the output name
	while (dash != string::npos)
	{
		outputName.replace(dash, 1, "_");
		dash = outputName.find_first_of('-');
	}
	int space = outputName.find_first_of(' ');	// the first space (if any) in the output name
	while (space != string::npos)
	{
		outputName.replace(space, 1, "_");
		space = outputName.find_first_of(' ');
	}
	Configuration::setOutputName(outputName);
	LOG(LogLevel::Info) << "Using output name " << outputName;

	LOG(LogLevel::Info) << "* Importing V2 save *";

	//	Parse V2 Save
	LOG(LogLevel::Info) << "Parsing save";
	obj = doParseFile(V2SaveFileName.c_str());
	if (obj == NULL)
	{
		LOG(LogLevel::Error) << "Could not parse file " << V2SaveFileName << ". File is likely missing.";
		exit(-1);
	}

	// Construct world from V2 save.
	LOG(LogLevel::Info) << "Building world";
	V2World sourceWorld(obj);

	// Read all localisations.
	LOG(LogLevel::Info) << "Reading localisation";
	V2Localisation localisation;
	localisation.ReadFromAllFilesInFolder(Configuration::getV2Path() + "\\localisation");
	for (auto itr: vic2Mods)
	{
		LOG(LogLevel::Debug) << "Reading mod localisation";
		localisation.ReadFromAllFilesInFolder(Configuration::getV2Path() + "\\mod\\" + itr + "\\localisation");
	}

	sourceWorld.setLocalisations(localisation);

	//// Resolve unit types
	///*log("Resolving unit types.\n");
	//printf("Resolving unit types.\n");
	//RegimentTypeMap rtm;
	//read.open("unit_strength.txt");
	//if (read.is_open())
	//{
	//	read.close();
	//	read.clear();
	//	log("\tReading unit strengths from unit_strength.txt\n");
	//	obj = doParseFile("unit_strength.txt");
	//	if (obj == NULL)
	//	{
	//		log("Could not parse file unit_strength.txt\n");
	//		exit(-1);
	//	}
	//	for (int i = 0; i < num_reg_categories; ++i)
	//	{
	//		AddCategoryToRegimentTypeMap(obj, (RegimentCategory)i, RegimentCategoryNames[i], rtm);
	//	}
	//}
	//else
	//{
	//	log("	Reading unit strengths from EU4 installation folder\n");
	//	struct _finddata_t unitFileData;
	//	intptr_t fileListing;
	//	if ( (fileListing = _findfirst( (EU4Loc + "\\common\\units\\*.txt").c_str(), &unitFileData)) == -1L)
	//	{
	//		log("	Could not open units directory.\n");
	//		return -1;
	//	}
	//	do
	//	{
	//		if (strcmp(unitFileData.name, ".") == 0 || strcmp(unitFileData.name, "..") == 0 )
	//		{
	//			continue;
	//		}
	//		string unitFilename = unitFileData.name;
	//		string unitName = unitFilename.substr(0, unitFilename.find_first_of('.'));
	//		AddUnitFileToRegimentTypeMap((EU4Loc + "\\common\\units"), unitName, rtm);
	//	} while(_findnext(fileListing, &unitFileData) == 0);
	//	_findclose(fileListing);
	//}
	//read.close();
	//read.clear();
	//sourceWorld.resolveRegimentTypes(rtm);*/


	// Merge nations
	LOG(LogLevel::Info) << "Merging nations";
	obj = doParseFile("merge_nations.txt");
	if (obj == NULL)
	{
		LOG(LogLevel::Error) << "Could not parse file merge_nations.txt";
		exit(-1);
	}
	mergeNations(sourceWorld, obj);
	
	// Parse HoI3 data files
	LOG(LogLevel::Info) << "Parsing HoI3 data";
	HoI3World destWorld;

	// Parse province mappings
	LOG(LogLevel::Info) << "Parsing province mappings";
	obj = doParseFile("province_mappings.txt");
	if (obj == NULL)
	{
		LOG(LogLevel::Error) << "Could not parse file province_mappings.txt";
		exit(-1);
	}
	provinceMapping			provinceMap;
	inverseProvinceMapping	inverseProvinceMap;
	resettableMap				resettableProvinces;
	initProvinceMap(obj, provinceMap, inverseProvinceMap, resettableProvinces);
	sourceWorld.checkAllProvincesMapped(inverseProvinceMap);

	// Get country mappings
	CountryMapping countryMap;
	countryMap.ReadRules("country_mappings.txt");
	countryMap.CreateMapping(sourceWorld, destWorld);

	//// Get adjacencies
	//LOG(LogLevel::Info) << "Importing adjacencies";
	//adjacencyMapping adjacencyMap = initAdjacencyMap();

	// Parse government mapping
	LOG(LogLevel::Info) << "Parsing governments mappings";
	initParser();
	obj = doParseFile("governmentMapping.txt");
	if (obj == NULL)
	{
		LOG(LogLevel::Error) << "Could not parse file governmentMapping.txt";
		exit(-1);
	}
	governmentMapping governmentMap;
	governmentMap = initGovernmentMap(obj->getLeaves()[0]);

	// Leaders
	map<int, int> leaderIDMap; // <V2, HoI3>

	// Parse government jobs
	LOG(LogLevel::Info) << "Parsing government jobs";
	initParser();
	obj = doParseFile("governmentJobs.txt");
	if (obj == NULL)
	{
		LOG(LogLevel::Error) << "Could not parse file governmentJobs.txt";
		exit(-1);
	}
	governmentJobsMap governmentJobs;
	initGovernmentJobTypes(obj->getLeaves()[0], governmentJobs);

	// parse names
	LOG(LogLevel::Info) << "Parsing names";
	namesMapping namesMap;
	vic2Mods = Configuration::getVic2Mods();
	for (auto itr: vic2Mods)
	{
		LOG(LogLevel::Debug) << "Reading mod cultures";
		obj = doParseFile((Configuration::getV2Path() + "\\mod\\" + itr + "\\common\\cultures.txt").c_str());
		if (obj != NULL)
		{
			initNamesMapping(obj, namesMap);
		}
	}
	obj = doParseFile((Configuration::getV2Path() + "\\common\\cultures.txt").c_str());
	if (obj != NULL)
	{
		initNamesMapping(obj, namesMap);
	}

	// parse portraits list
	LOG(LogLevel::Info) << "Parsing portraits list";
	portraitMapping portraitMap;
	obj = doParseFile("portraits.txt");
	if (obj != NULL)
	{
		initPortraitMapping(obj, portraitMap);
	}

	// parse culture mapping
	LOG(LogLevel::Info) << "Parsing culture mappings";
	obj = doParseFile("culture_map.txt");
	if (obj == NULL)
	{
		LOG(LogLevel::Error) << "Could not parse file culture_map.txt";
		exit(-1);
	}
	if (obj->getLeaves().size() < 1)
	{
		LOG(LogLevel::Error) << "Failed to parse culture_map.txt";
		return 1;
	}
	cultureMapping cultureMap;
	cultureMap = initCultureMap(obj->getLeaves()[0]);

	// Convert
	LOG(LogLevel::Info) << "Converting countries";
	destWorld.convertCountries(sourceWorld, countryMap, governmentMap, inverseProvinceMap, leaderIDMap, localisation, governmentJobs, namesMap, portraitMap, cultureMap);
	LOG(LogLevel::Info) << "Converting provinces";
	destWorld.convertProvinces(sourceWorld, provinceMap, countryMap);
	LOG(LogLevel::Info) << "Converting diplomacy";
	destWorld.convertDiplomacy(sourceWorld, countryMap);
	LOG(LogLevel::Info) << "Converting techs";
	destWorld.convertTechs(sourceWorld);
	LOG(LogLevel::Info) << "Converting armies and navies";
	destWorld.convertArmies(sourceWorld, inverseProvinceMap, leaderIDMap);
	LOG(LogLevel::Info) << "Setting up factions";
	destWorld.configureFactions(sourceWorld, countryMap);

	// Output results
	LOG(LogLevel::Info) << "Outputting mod";
	system("%systemroot%\\System32\\xcopy blankMod output /E /Q /Y /I");

	FILE* modFile;	// the .mod file for this mod
	if (fopen_s(&modFile, ("Output\\" + Configuration::getOutputName() + ".mod").c_str(), "w") != 0)
	{
		LOG(LogLevel::Error) << "Could not create .mod file";
		exit(-1);
	}
	fprintf(modFile, "name = \"Converted - %s\"\n", Configuration::getOutputName().c_str());
	// BE: Use full path for people whose mod directory is in different drive from HoI3 installation directory. i.e., me.
	fprintf(modFile, "path = \"%s/mod/%s\"\n", Configuration::getHoI3DocumentsPath().c_str(), Configuration::getOutputName().c_str());
	fprintf(modFile, "user_dir = \"%s_user_dir\"\n", Configuration::getOutputName().c_str());
	fprintf(modFile, "replace = \"history/countries\"\n");
	fprintf(modFile, "replace = \"history/diplomacy\"\n");
	fprintf(modFile, "replace = \"history/provinces\"\n");
	fprintf(modFile, "replace = \"script\"\n");
	fprintf(modFile, "replace_path = \"events\"\n");
	fprintf(modFile, "replace_path = \"decisions\"\n");
	fclose(modFile);
	string renameCommand = "move /Y output\\output output\\" + Configuration::getOutputName();	// the command to rename the mod correctly
	system(renameCommand.c_str());
	LOG(LogLevel::Info) << "Copying flags";
	destWorld.copyFlags(sourceWorld, countryMap);

	LOG(LogLevel::Info) << "Outputting world";
	destWorld.output();

	LOG(LogLevel::Info) << "* Conversion complete *";
	return 0;
}


int main(const int argc, const char * argv[])
{
	try
	{
		const char* const defaultV2SaveFileName = "input.v2";	// the default name for a save to convert
		string V2SaveFileName;												// the actual name for the save to convert
		if (argc >= 2)
		{
			V2SaveFileName = argv[1];
			LOG(LogLevel::Info) << "Using input file " << V2SaveFileName;
		}
		else
		{
			V2SaveFileName = defaultV2SaveFileName;
			LOG(LogLevel::Info) << "No input file given, defaulting to " << defaultV2SaveFileName;
		}
		return ConvertV2ToHoI3(V2SaveFileName);
	}
	catch (const std::exception& e)	// catch any errors
	{
		LOG(LogLevel::Error) << e.what();
		return -1;
	}
}
