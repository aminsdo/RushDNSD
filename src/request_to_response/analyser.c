#include "analyser.h"

dns *analyser(dns *request, zone *soa, bin_tree *tree){
	if (request_checker(request) == 0)
	{
		// Invalid request
		// forge response for invqlid request
		return make_valid_response(request, soa, tree);

	}
	else
	{
		// valid request
		return make_valid_response(request, soa, tree);
	}
	return request;
}
