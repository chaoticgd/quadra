#ifndef _QUADRA_ARCHITECTURE_H
#define _QUADRA_ARCHITECTURE_H

#include <decompile/cpp/sleigh_arch.hh>

class QuadraArchitecture : public SleighArchitecture {
public:
	QuadraArchitecture(
		const std::string& fname,
		const std::string& targ,
		std::ostream* estream);

private:
	void buildLoader(DocumentStorage& store) override;
	void resolveArchitecture() override;
	void postSpecFile() override;

public:
	void saveXml(std::ostream& s) const override;
	void restoreXml(DocumentStorage& store) override;
};

#endif
