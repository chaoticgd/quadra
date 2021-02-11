#ifndef _QUADRA_ARCHITECTURE_H
#define _QUADRA_ARCHITECTURE_H

#include <decompile/cpp/sleigh_arch.hh>

class QuadraArchitecture : public SleighArchitecture {
public:
	QuadraArchitecture(
		const std::string& fname,
		const std::string& targ,
		std::ostream* estream);
	
	std::string return_register();
	std::vector<std::string> syscall_argument_registers();
	std::string syscall_return_register();

private:
	void buildLoader(DocumentStorage& store) override;
	void resolveArchitecture() override;
	void postSpecFile() override;

public:
	void saveXml(std::ostream& s) const override;
	void restoreXml(DocumentStorage& store) override;
};

#endif
