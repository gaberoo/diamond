/****
Copyright (c) 2014, University of Tuebingen
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
****
Author: Benjamin Buchfink
****/

#ifndef SHAPE_CONFIG_H_
#define SHAPE_CONFIG_H_

#include "shape.h"

extern const char* shape_codes[Const::index_modes][Const::max_shapes];

class shape_config
{

public:
	
	shape_config():
		n_ (0),
		mode_ (0)
	{ }

	shape_config(unsigned mode, unsigned count):
		n_ (0),
		mode_ (mode)
	{
		unsigned maxShapes = count == 0 ? Const::max_shapes : count;
		for(unsigned i=0;i<maxShapes;++i)
			if(shape_codes[mode_][i])
				shapes_[n_++] = shape (shape_codes[mode_][i], i);
	}

	unsigned count() const
	{ return n_; }

	const shape& get_shape(unsigned i) const
	{ return shapes_[i]; }

	unsigned mode() const
	{ return mode_; }

	friend std::ostream& operator<<(std::ostream&s, const shape_config &cfg)
	{
		for (unsigned i = 0; i < cfg.n_; ++i)
			s << cfg.shapes_[i] << (i<cfg.n_-1?",":"");
		return s;
	}

private:

	shape shapes_[Const::max_shapes];
	unsigned n_, mode_;

};

extern shape_config shapes;

#endif /* SHAPE_CONFIG_H_ */
