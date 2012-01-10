#ifndef COMMANDS_H
#define COMMANDS_H

#include "common.h"
#include "context.h"
#include "agenda.h"

namespace es3 {
	extern int term_width;

	int do_rsync(context_ptr context, const stringvec& params,
			 agenda_ptr ag, bool help);

}; //namespace es3

#endif //COMMANDS_H
