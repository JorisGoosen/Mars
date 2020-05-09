#include <RenderSubBuffer.h>
#include <Helpers.h>

#define AANTAL_DINGEN 100

class Dingen
{
public:
	Dingen();

	void tekenJezelf() const;

private:
	void genereer(bool PrettyIcoInsteadOfJensens = true);

	ArrayOfStructOfArrays			* _dingArray  = nullptr;
	RenderSubBuffer<float>			* _dingPunten = nullptr;
	std::vector<glm::uint32>	  	  _dingDriehk;
};