#include "Engine/Application.h"

int main(){

	Hex::ApplicationSpecification spec;
	spec.name = "Sandbox";
	spec.height = 500;
	spec.width = 800;

	const auto application = Hex::Application(spec);
	
	return 0;
}
