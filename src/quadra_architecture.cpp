#include "quadra_architecture.h"

#include <assert.h>

#include "elf_loader.h"

QuadraArchitecture::QuadraArchitecture(
	const std::string& fname,
	const std::string& targ,
	std::ostream* estream)
	: SleighArchitecture(fname, targ, estream)
{}

void QuadraArchitecture::buildLoader(DocumentStorage& store)
{
	collectSpecFiles(std::cerr);
	loader = new ElfLoader(getFilename());
}

void QuadraArchitecture::resolveArchitecture()
{
	SleighArchitecture::resolveArchitecture();
}

void QuadraArchitecture::postSpecFile()
{
	Architecture::postSpecFile();
	//((LoadImageBfd*) loader)->attachToSpace(getDefaultCodeSpace());
}

void QuadraArchitecture::saveXml(std::ostream& s) const
{
	assert(0);
}

void QuadraArchitecture::restoreXml(DocumentStorage& store)
{
	assert(0);
}
