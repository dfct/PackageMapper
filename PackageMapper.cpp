//PackageMapper
//Map Windows Operating Systems from top-level packages to bottom-level components

#include <io.h>													//Used for a _setmode call to allow outputting in unicode
#include <fcntl.h>												//Used for a _setmode call to allow outputting in unicode
#include <iostream>												//Console output
#include <string>												//std::string
#include <vector>												//std::vector
#include <algorithm>											//std::transform
#include <Windows.h>											//Basic Windows definitions & functions
#include "pugixml.hpp"											//XML parsing

using namespace pugi;											//Pugi XML namespace
using namespace std;											//Console output, strings, etc

struct assemblyIdentity{
	wstring name;
	wstring language;
	wstring architecture;
	wstring version;
};

enum assemblyType {
	unknown, root, package, driver,
	deployment, component
};

struct assembly {												//Struct to contain a package's XML data and file name
	assemblyType aType = unknown;
	xml_document * assemblyXML;
	wstring assemblyFileName;
	assemblyIdentity aIdentity;
};

vector<assembly> packageList;									//And a vector to store the structs
vector<assembly> manifestList;									//And a vector to store the structs

enum fileType { packages, manifests };


void showHelp();												//Print command line help text
void findRoots(wchar_t packagesDir[]);							//
void loadXMLFiles(wchar_t directory[], fileType fType);			//






int wmain(int argc, wchar_t * argv[])
{
	//Allow output of unicode characters 
	_setmode(_fileno(stdout), _O_U16TEXT);
	wcout << endl;


	//Check for the presence of switches
	if (argc != 3)
	{
		showHelp();
		return 0;
	}

	if (wcscmp(argv[1], L"/findRoots") == 0)
	{
		findRoots(argv[2]);
		return 0;
	}
	else
	{
		wcout << "Unrecognized switch. Run /? for help.\n";
		return 0;
	}



	return 0;

}

void findRoots(wchar_t packagesDir[])
{
	loadXMLFiles(packagesDir, packages);

	

	for (unsigned int i = 0; i < packageList.size(); i++)											//For each assembly in the package list
	{
		for (xml_node assmPkg : packageList[i].assemblyXML->child(L"assembly").children(L"package"))//For each package node in the assembly
		{
			for (xml_node update : assmPkg.children(L"update"))										//For each update node in the package node
			{
				for (xml_node refPackage : update.children(L"package"))								//For each package node in the update node
				{
					
					xml_node tempIdentity = refPackage.child(L"assemblyIdentity");					//Access the referenced assemblyIdentity node			
					xml_attribute tempName = tempIdentity.attribute(L"name");
					xml_attribute tempVersion = tempIdentity.attribute(L"version");
					xml_attribute tempLang = tempIdentity.attribute(L"language");
					xml_attribute tempArch = tempIdentity.attribute(L"processorArchitecture");


					assemblyIdentity refAssembly;													//Fill in an assemblyIdentity struct
					refAssembly.name = tempName.as_string();
					refAssembly.version = tempVersion.as_string();
					refAssembly.language = tempLang.as_string();
					refAssembly.architecture = tempArch.as_string();


																									//Lowercase the values for name, language, and architecture
					
					std::transform(refAssembly.name.begin(), refAssembly.name.end(), refAssembly.name.begin(), ::tolower);
					std::transform(refAssembly.language.begin(), refAssembly.language.end(), refAssembly.language.begin(), ::tolower);
					std::transform(refAssembly.architecture.begin(), refAssembly.architecture.end(), refAssembly.architecture.begin(), ::tolower);



					for (unsigned int j = 0; j < packageList.size(); j++)							//Compare the struct against each assembly in the package List
					{
						if (i == j)
						{
							continue;
						}

						if (refAssembly.name.compare(packageList[j].aIdentity.name) == 0)
						{
							if (refAssembly.version.compare(packageList[j].aIdentity.version) == 0)
							{
								if (refAssembly.architecture.compare(packageList[j].aIdentity.architecture) == 0)
								{
									if (refAssembly.language.compare(packageList[j].aIdentity.language) == 0)
									{
										packageList[j].aType = package;								//Set the matching assembly to type package and exit the loop
										break;
									}
								}
							}
						}																		
					}
				}
			}
		}
	}
	//All packages referenced by other packages have been marked as 'package'

	vector<wstring> rootPackages;
	wcout << "Root packages: " << endl;
	for (unsigned int i = 0; i < packageList.size(); i++)
	{
		if (packageList[i].aType == unknown)
		{
			packageList[i].aType = root;
			rootPackages.push_back(packageList[i].assemblyFileName);
			wcout << "\t" << packageList[i].assemblyFileName.c_str() << endl;
		}
	}

	return;
}

void loadXMLFiles(wchar_t wcharDirectory[], fileType fType)
{
	wstring directory = wcharDirectory;				//String for the directory to search in
	wstring searchStr;								//Search string for the Find* function

	if (directory.back() != L'\\')					//If the directory path is missing a trailing \, add it now
	{
		directory += L'\\';
	}

	if (fType == packages)							//Set the search string appropriately for either packages or manifests
	{
		searchStr = directory + L"*.mum";
	}
	else if (fType == manifests)
	{
		searchStr = directory + L"*.manifest";
	}
	

	wcout << "Loading files matching: " << searchStr.c_str() << endl;




	HANDLE findHandle = INVALID_HANDLE_VALUE;		//Handle for the Find* functions
	WIN32_FIND_DATAW foundFile;						//Object for found files
	int foundFileCount = 0;							//Int to count each found for output later
	int fileLoadErrors = 0;							//Int to track errors



	//Get a Find handle for files matching the search string
	findHandle = FindFirstFileW(searchStr.c_str(),	&foundFile);

	if (findHandle == INVALID_HANDLE_VALUE)
	{
		wcout << "Error opening Find handle. Error code: " << GetLastError() << endl;
		return;
	}

	
	
	//Process each found mum file until FindNextFile returns a nonzero value
	do
	{
		foundFileCount++;											//Increment the found file count
		assembly tempAssemblyObject;								//Create a temporary assembly object
		wstring fullFilePath = directory + foundFile.cFileName;		//Merge the mum file name and package path to get a full file path for loading			
		tempAssemblyObject.assemblyFileName = foundFile.cFileName;	//Set the file name
		tempAssemblyObject.assemblyXML = new xml_document;			//And get a xml_document object behind the pointer


		
		//Attempt to load the file
		xml_parse_result result = tempAssemblyObject.assemblyXML->load_file(fullFilePath.c_str());

		if (result.status != status_ok)								
		{
			wcout << "Could not load file. Error: " << result.description() << endl
				  << "File: " << directory.c_str() << foundFile.cFileName << endl;		
			fileLoadErrors++;
			continue;			
		}

		xml_node tempIdentity = tempAssemblyObject.assemblyXML->child(L"assembly").child(L"assemblyIdentity");
		xml_attribute tempName = tempIdentity.attribute(L"name");
		xml_attribute tempVersion = tempIdentity.attribute(L"version");
		xml_attribute tempLang = tempIdentity.attribute(L"language");
		xml_attribute tempArch = tempIdentity.attribute(L"processorArchitecture");

		if (!tempIdentity || !tempName || !tempVersion || !tempLang || !tempArch)
		{
			wcout << "Could not load file. Error: Unrecognized assembly format." << endl
				<< "File: " << directory.c_str() << foundFile.cFileName << endl;
			fileLoadErrors++;
			continue;
		}

		//Store the assembly identity info in the struct
		tempAssemblyObject.aIdentity.name = tempName.as_string();
		tempAssemblyObject.aIdentity.version = tempVersion.as_string();
		tempAssemblyObject.aIdentity.language = tempLang.as_string();
		tempAssemblyObject.aIdentity.architecture = tempArch.as_string();

		//Lowercase the values for name, language, and architecture
		std::transform(tempAssemblyObject.aIdentity.name.begin(), tempAssemblyObject.aIdentity.name.end(), tempAssemblyObject.aIdentity.name.begin(), ::tolower);
		std::transform(tempAssemblyObject.aIdentity.language.begin(), tempAssemblyObject.aIdentity.language.end(), tempAssemblyObject.aIdentity.language.begin(), ::tolower);
		std::transform(tempAssemblyObject.aIdentity.architecture.begin(), tempAssemblyObject.aIdentity.architecture.end(), tempAssemblyObject.aIdentity.architecture.begin(), ::tolower);


		//Add the object to the global vector
		if (fType == packages)							
		{
			packageList.push_back(tempAssemblyObject);
		}
		else if (fType == manifests)
		{
			manifestList.push_back(tempAssemblyObject);
		}
	}
	while (FindNextFileW(findHandle, &foundFile) != 0);

	//Close the Find handle
	FindClose(findHandle);


	//Output result
	wcout << "Loaded " << foundFileCount - fileLoadErrors << " of " << foundFileCount << " successfully." << endl;

	if (fileLoadErrors != 0)
	{
		wchar_t answer;
		wcout << "Not all files were loaded successfully. Would you like to continue? [y|n] ";
		wcin >> answer;

		wcout << endl;

		if (answer != 'y' || answer != 'Y')
		{
			ExitProcess(0);
		}
	}



	return;
}























void showHelp()
{
	wcout << endl
		<< "Pass the location of the Windows directory of the operating system to map." << endl
		<< endl
		<< "PackageMapper.exe /findParents \"C:\\Mount\\Windows\\servicing\\Packages\"" << endl
		<< endl;

	return;
}











/*





	//Initializing the assemblyMaps to add things to
	assemblyMap.load(L"<AssemblyTree></AssemblyTree>");
	assemblyMapWithChildren.load(L"<AssemblyTree></AssemblyTree>");		
	assemblyOrphans.load(L"<AssemblyTree></AssemblyTree>");

	//This avoids string contatenation for now
	SetCurrentDirectoryW(L"C:\\temp\\mount\\Windows\\servicing\\Packages\\");

	//Open a Find handle to loop through package mum files with
	WIN32_FIND_DATAW findData;
	HANDLE findHandle = FindFirstFileW(L"C:\\temp\\mount\\Windows\\servicing\\Packages\\*.mum", &findData);
	if (findHandle == INVALID_HANDLE_VALUE)
		return GetLastError();

	wcout << "Initializing Package Map (Mums)...\n";

	//Loop through each one until FindNextFile returns nonzero. Call mapPackages for each
	do initialPackageMapping(findData.cFileName);
	while (FindNextFileW(findHandle, &findData) != 0);

	//Close the Find handle
	FindClose(findHandle);

	manifest = true;

	//This avoids string contatenation for now
	SetCurrentDirectoryW(L"C:\\temp\\mount\\Windows\\winsxs\\manifests\\");

	//Open a Find handle to loop through package mum files with
	findHandle = FindFirstFileW(L"C:\\temp\\mount\\Windows\\winsxs\\manifests\\*.manifest", &findData);
	if (findHandle == INVALID_HANDLE_VALUE)
		return GetLastError();

	wcout << "Initializing Package Map (Manifests)...\n";

	//Loop through each one until FindNextFile returns nonzero. Call mapPackages for each
	do initialPackageMapping(findData.cFileName);
	while (FindNextFileW(findHandle, &findData) != 0);

	//Close the Find handle
	FindClose(findHandle);




	assemblyMap.save_file(L"C:\\temp\\file.xml");
	assemblyMapWithChildren.save_file(L"C:\\temp\\file_children.xml");

	SetCurrentDirectoryW(L"C:\\temp\\mount\\Windows\\servicing\\Packages\\");
	wcout << "Mapping package children & orphans...\n";
	for (xml_node eachMumFile : assemblyMapWithChildren.child(L"AssemblyTree").children())
		mapMums(eachMumFile);

	assemblyMapWithChildren.save_file(L"C:\\temp\\file_children.xml");
	assemblyOrphans.save_file(L"C:\\temp\\file_orphans.xml");


	wcout << "Culling duplicate root nodes...\n";
	for (xml_node eachMumNode : assemblyMapWithChildren.child(L"AssemblyTree").children())
	{
		for (xml_node eachMumNodeWithChildren : eachMumNode.children())
		{
			cullMums(eachMumNodeWithChildren);
		}
	}

	assemblyMapWithChildren.save_file(L"C:\\temp\\file_children.xml");

	SetCurrentDirectoryW(L"C:\\temp\\mount\\Windows\\winsxs\\manifests\\");
	wcout << "Mapping manifest children and orphans...\n";
	for (xml_node eachMumFile : assemblyMapWithChildren.child(L"AssemblyTree").children())
		mapManifests(eachMumFile);

	//Write out the assemblies

	assemblyMapWithChildren.save_file(L"C:\\temp\\file_children.xml");

	wcout << "Done!\n";
	return 0;

*/


void initialPackageMapping(wchar_t * mumFile);
void mapMums(xml_node mumNode);
void mapManifests(xml_node manifests);
void findFile(xml_node childAssemblyIdentity, wchar_t * foundFile);
void cullMums(xml_node MumNode);

xml_document assemblyMap;
xml_document assemblyMapWithChildren;
xml_document assemblyOrphans;

bool manifest = false;
int testing = 0;


void initialPackageMapping(wchar_t * mumFile)
{
	xml_node assemblyTree = assemblyMap.child(L"AssemblyTree");											//Open the initial XML node
	xml_node assemblyTree2 = assemblyMapWithChildren.child(L"AssemblyTree");
	xml_document mum;																					//Create a xml_document for the mumFile passed in
	xml_parse_result result = mum.load_file(mumFile);													//Load the mumFile to the xml_document
	if (result.status != status_ok)																		//Check load status
	{
		std::wcout << "Could not parse package mum. Error: " << result.description() << "\n";			//If we couldn't load the file, output that
		return;																							//And return
	}

	assemblyTree.append_child(mumFile);																	//Create a new MumFileName child node
	if (manifest == false)
		assemblyTree2.append_child(mumFile);																//Create it in the other map too

	xml_node assemblyTreeMumFile = assemblyTree.child(mumFile);											//AssemblyTree Node -> Mum File Node
	xml_node mumAssembly = mum.child(L"assembly");														//Document -> First node

	assemblyTreeMumFile.append_child(L"assemblyIdentity");												//Create a new assemblyIdentity child node
	xml_node assemblyTreeMumFileAssemblyIdentity = assemblyTreeMumFile.child(L"assemblyIdentity");		//Mum File Node -> AssemblyIdentity Node

	for (xml_attribute attributes : mumAssembly.child(L"assemblyIdentity").attributes())				//Loop through all of the attributes
		assemblyTreeMumFileAssemblyIdentity.append_attribute(attributes.name()) = attributes.value();	//Copying them to the new assemblyIdentity child node

	return;
}

void mapMums(xml_node mumNode)
{

	int nameLength = wcslen((wchar_t *)mumNode.name());
	if (mumNode.name()[nameLength - 1] == L't')
		return;

	xml_document mum;																					//Create a xml_document for the mumFile passed in

	xml_parse_result result = mum.load_file(mumNode.name());											//Load the mumFile to the xml_document
	if (result.status != status_ok)																		//Check load status
	{
		std::wcout << "Could not parse package mum " << mumNode.name() << ". Error: " << result.description() << "\n";		//If we couldn't load the file, output that
		return;
	}

	xml_node mumAssembly = mum.child(L"assembly");														//Document -> First node

	for (xml_node package : mumAssembly.children(L"package"))											//For each child matching "package", loop
	{
		for (xml_node update : package.children(L"update"))												//For each child matching "update", loop
		{
			wchar_t * threeTypes[3] = { L"package", L"component", L"driver" };							//The three possible update types we need
			for (int i = 0; i < 3; i++)																	//For each of the three
			{
				for (xml_node childOfUpdate : update.children(threeTypes[i]))							//Loop through selecting them
				{

					xml_node childAssemblyIdentity = childOfUpdate.child(L"assemblyIdentity");			//Select the assemblyIdentity node
					wchar_t childMumFileName[260] = { '\0' };
					findFile(childAssemblyIdentity, childMumFileName);									//Pass it to my comparison function
					if (childMumFileName[0] == '\0')													//Check if it matched
						break;

					mumNode.append_child(childMumFileName);
					xml_node recurse = mumNode.child(childMumFileName);
					mapMums(recurse);
				}
			}
		}
	}

	/*
	for (xml_node dependency : mumAssembly.child(L"dependency"))
	{
		for (xml_node dependentAssembly : dependency.children())
		{
			xml_node childAssemblyIdentity = dependentAssembly.child(L"assemblyIdentity");		//Select the assemblyIdentity node
			wchar_t childManifestFileName[260] = { '\0' };
			findFile(childAssemblyIdentity, childManifestFileName);								//Pass it to my comparison function
			if (childManifestFileName[0] == '\0')												//Check if it matched
				break;

			mumNode.append_child(childManifestFileName);
			xml_node recurse = mumNode.child(childManifestFileName);
			mapManifests(recurse);
		}
	}
	*/
	return;
}

void cullMums(xml_node mumNode)
{
	assemblyMapWithChildren.child(L"AssemblyTree").remove_child(mumNode.name());

	if (mumNode.first_child())
		cullMums(mumNode.first_child());
	
	if (mumNode.next_sibling())
		cullMums(mumNode.next_sibling());

	return;
}

void mapManifests(xml_node manifestNode)
{
	//Is it a mum? Chain further
	
	int nameLength = wcslen((wchar_t *)manifestNode.name());
	if (manifestNode.name()[nameLength - 1] == L'm')
	{
		if (manifestNode.first_child())
			mapManifests(manifestNode.first_child());

		if (manifestNode.next_sibling())
			mapManifests(manifestNode.next_sibling());

		return;
	}

	xml_document manifest;																				//Create a xml_document for the mumFile passed in
	
	xml_parse_result result = manifest.load_file(manifestNode.name());									//Load the mumFile to the xml_document
	if (result.status != status_ok)																		//Check load status
	{
		std::wcout << "Could not parse package manifest " << manifestNode.name() << ". Error: " << result.description() << "\n";		//If we couldn't load the file, output that
		return;
	}

	xml_node manifestAssembly = manifest.child(L"assembly");													//Document -> First node

	for (xml_node dependency : manifestAssembly.children(L"dependency"))
	{
		for (xml_node dependentAssembly : dependency.children())
		{
			xml_node childAssemblyIdentity = dependentAssembly.child(L"assemblyIdentity");			//Select the assemblyIdentity node
			wchar_t childManifestFileName[260] = { '\0' };
			findFile(childAssemblyIdentity, childManifestFileName);									//Pass it to my comparison function
			if (childManifestFileName[0] == '\0')													//Check if it matched
				break;

			manifestNode.append_child(childManifestFileName);
			xml_node recurse = manifestNode.child(childManifestFileName);
			mapManifests(recurse);
		}
	}

	return;
}

void findFile(xml_node childAssemblyIdentity, wchar_t * foundFile)
{
	
	xml_node allMappedTreeAssemblies = assemblyMap.child(L"AssemblyTree");

	int matchCount = 0;
	for (xml_node mappedAssembly : allMappedTreeAssemblies.children())	
	{
		bool match = true;

		//Check the assemblyIdentity against what we were passed
		/*
		for (xml_attribute matchMe : childAssemblyIdentity.attributes())
		{
			xml_attribute mappedAssemblyAttribute = mappedAssembly.child(L"assemblyIdentity").attribute(matchMe.name());
			if (_wcsnicmp(mappedAssemblyAttribute.value(), matchMe.value(), size_t(260)) != 0)					//Check it against the assembly we're trying to find
				match = false;
		}
		*/

		/*
		for (xml_attribute mappedAssemblyAttribute : mappedAssembly.child(L"assemblyIdentity").attributes())	//For every attribute in the mapped mum
		{
			xml_attribute sourceAttribute = childAssemblyIdentity.attribute(mappedAssemblyAttribute.name());
			if (sourceAttribute == NULL)
				wcout << "Missing " << mappedAssemblyAttribute.name() << "\n";

			if (_wcsnicmp(mappedAssemblyAttribute.value(), sourceAttribute.value(), size_t(260)) != 0)					//Check it against the assembly we're trying to find
				match = false;

		}
		*/

		

		if (_wcsnicmp(childAssemblyIdentity.attribute(L"version").value(), mappedAssembly.child(L"assemblyIdentity").attribute(L"version").value(), size_t(260)) != 0)					//Check it against the assembly we're trying to find
			match = false;

		if (_wcsnicmp(childAssemblyIdentity.attribute(L"architecture").value(), mappedAssembly.child(L"assemblyIdentity").attribute(L"architecture").value(), size_t(260)) != 0)					//Check it against the assembly we're trying to find
			match = false;

		if (_wcsnicmp(childAssemblyIdentity.attribute(L"language").value(), mappedAssembly.child(L"assemblyIdentity").attribute(L"language").value(), size_t(260)) != 0)					//Check it against the assembly we're trying to find
			match = false;

		if (_wcsnicmp(childAssemblyIdentity.attribute(L"name").value(), mappedAssembly.child(L"assemblyIdentity").attribute(L"name").value(), size_t(260)) != 0)					//Check it against the assembly we're trying to find
			match = false;

		//If we make it through the loop without returning the fail result
		//It's a match
		if (match == true)
		{
			matchCount++;
			wcscpy_s(foundFile, (rsize_t)260, (wchar_t *)mappedAssembly.name());
			return;
		}
	}

//	if (matchCount == 1)
//		return;
//	else if (matchCount > 1)
//		wcout << "Problem. Match count is greater than 1: " << matchCount << "\n";


	//We've gone through all of them and haven't found it.

	xml_node assemblyTreeOrphans = assemblyOrphans.child(L"AssemblyTree");
	assemblyTreeOrphans.append_child(L"assemblyIdentity");									//Create a new assemblyIdentity child node
	xml_node assemblyOrphansAssembly = assemblyTreeOrphans.last_child();					//Mum File Node -> AssemblyIdentity Node
	
	for (xml_attribute attributes : childAssemblyIdentity.attributes())						//Loop through all of the attributes
		assemblyOrphansAssembly.append_attribute(attributes.name()) = attributes.value();	//Copying them to the new assemblyIdentity child node


	return;
}
