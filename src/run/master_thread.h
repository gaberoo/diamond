/****
Copyright (c) 2014-2016, University of Tuebingen, Benjamin Buchfink
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
****/

#ifndef MASTER_THREAD_H_
#define MASTER_THREAD_H_

#include <iostream>
#include <limits>
#include "../data/reference.h"
#include "../data/queries.h"
#include "../basic/statistics.h"
#include "../basic/shape_config.h"
#include "../output/join_blocks.h"
#include "../align/align_queries.h"
#include "../search/align_range.h"
#include "../util/seq_file_format.h"
#include "../data/load_seqs.h"
#include "../search/setup.h"

using std::endl;
using std::cout;

struct Search_context
{
	Search_context(unsigned sid, const sorted_list &ref_idx, const sorted_list &query_idx):
		sid (sid),
		ref_idx (ref_idx),
		query_idx (query_idx)
	{ }
	void operator()(unsigned thread_id, unsigned seedp) const
	{
		Statistics stat;
		align_partition(seedp,
				stat,
				sid,
				ref_idx.get_partition_cbegin(seedp),
				query_idx.get_partition_cbegin(seedp),
				thread_id);
		statistics += stat;
	}
	const unsigned sid;
	const sorted_list &ref_idx;
	const sorted_list &query_idx;
};

void process_shape(unsigned sid,
		Timer &timer_mapping,
		unsigned query_chunk,
		char *query_buffer,
		char *ref_buffer)
{
	using std::vector;

	::partition<unsigned> p (Const::seedp, config.lowmem);
	for(unsigned chunk=0;chunk < p.parts; ++chunk) {

		message_stream << "Processing query chunk " << query_chunk << ", reference chunk " << current_ref_block << ", shape " << sid << ", index chunk " << chunk << '.' << endl;
		const seedp_range range (p.getMin(chunk), p.getMax(chunk));
		current_range = range;

		task_timer timer ("Building reference index", true);
		sorted_list ref_idx (ref_buffer,
				*ref_seqs::data_,
				shapes.get_shape(sid),
				ref_hst.get(config.index_mode, sid),
				range);
		ref_seqs::get_nc().build_masking(sid, range, ref_idx);

		timer.go("Building query index");
		timer_mapping.resume();
		sorted_list query_idx (query_buffer,
				*query_seqs::data_,
				shapes.get_shape(sid),
				query_hst->get(config.index_mode, sid),
				range);
		timer.finish();

		timer.go("Searching alignments");
		Search_context context (sid, ref_idx, query_idx);
#ifdef SIMPLE_SEARCH
		launch_scheduled_thread_pool(context, Const::seedp, config.threads_);
#else
		launch_scheduled_thread_pool(context, Const::seedp, 1);
#endif
	}
	timer_mapping.stop();
}

void run_ref_chunk(Database_file &db_file,
		Timer &timer_mapping,
		Timer &total_timer,
		unsigned query_chunk,
		pair<size_t,size_t> query_len_bounds,
		char *query_buffer,
		DAA_output &master_out,
		vector<Temp_file> &tmp_file)
{
	task_timer timer ("Loading reference sequences", true);
	ref_seqs::data_ = new Masked_sequence_set (db_file);
	ref_ids::data_ = new String_set<0> (db_file);
	ref_hst.load(db_file);
	setup_search_params(query_len_bounds, ref_seqs::data_->letters());
	ref_map.init((unsigned)ref_seqs::get().get_length());

	timer.go("Allocating buffers");
	char *ref_buffer = sorted_list::alloc_buffer(ref_hst);

	timer.go("Initializing temporary storage");
	timer_mapping.resume();
	Trace_pt_buffer::instance = new Trace_pt_buffer (query_seqs::data_->get_length()/align_mode.query_contexts,
			config.tmpdir,
			config.mem_buffered());
	timer.finish();
	timer_mapping.stop();

	for(unsigned i=0;i<shapes.count();++i)
		process_shape(i, timer_mapping, query_chunk, query_buffer, ref_buffer);

	/*timer.go("Closing temporary storage");
	Trace_pt_buffer::instance->close();*/

	timer.go("Deallocating buffers");
	delete[] ref_buffer;

	timer_mapping.resume();
	Output_stream* out;
	if(ref_header.n_blocks > 1) {
		timer.go ("Opening temporary output file");
		tmp_file.push_back(Temp_file ());
		out = new Output_stream (tmp_file.back());
	} else
		out = &master_out.stream();

	timer.go("Computing alignments");
	align_queries(*Trace_pt_buffer::instance, out);
	delete Trace_pt_buffer::instance;

	if(ref_header.n_blocks > 1) {
		delete out;
	}
	timer_mapping.stop();

	timer.go("Deallocating reference");
	delete ref_seqs::data_;
	delete ref_ids::data_;
	timer.finish();
}

void run_query_chunk(Database_file &db_file,
		Timer &timer_mapping,
		Timer &total_timer,
		unsigned query_chunk,
		pair<size_t,size_t> query_len_bounds,
		DAA_output &master_out)
{
	task_timer timer ("Allocating buffers", true);
	char *query_buffer = sorted_list::alloc_buffer(*query_hst);
	vector<Temp_file> tmp_file;
	timer.finish();

	db_file.rewind();
	for(current_ref_block=0;current_ref_block<ref_header.n_blocks;++current_ref_block)
		run_ref_chunk(db_file, timer_mapping, total_timer, query_chunk, query_len_bounds, query_buffer, master_out, tmp_file);

	timer.go("Deallocating buffers");
	timer_mapping.resume();
	delete[] query_buffer;

	if(ref_header.n_blocks > 1) {
		timer.go("Joining output blocks");
		join_blocks(ref_header.n_blocks, master_out, tmp_file);
	}

	timer.go("Deallocating queries");
	delete query_seqs::data_;
	delete query_ids::data_;
	delete query_source_seqs::data_;
	timer_mapping.stop();
}

void master_thread(Database_file &db_file, Timer &timer_mapping, Timer &total_timer)
{
	task_timer timer ("Opening the input file", true);
	timer_mapping.start();
	const Sequence_file_format *format_n (guess_format(config.query_file));
	Compressed_istream query_file (config.query_file);
	current_query_chunk=0;

	timer.go("Opening the output file");
	DAA_output master_out;
	timer_mapping.stop();
	timer.finish();

	for(;;++current_query_chunk) {
		task_timer timer ("Loading query sequences", true);
		timer_mapping.resume();
		size_t n_query_seqs;
		n_query_seqs = load_seqs(query_file, *format_n, &query_seqs::data_, query_ids::data_, query_source_seqs::data_, (size_t)(config.chunk_size * 1e9));
		if(n_query_seqs == 0)
			break;
		timer.finish();
		query_seqs::data_->print_stats();

		if(align_mode.sequence_type == amino_acid && config.seg == "yes") {
			timer.go("Running complexity filter");
			Complexity_filter::get().run(*query_seqs::data_);
		}

		timer.go("Building query histograms");
		query_hst = auto_ptr<seed_histogram> (new seed_histogram (*query_seqs::data_));
		const pair<size_t,size_t> query_len_bounds = query_seqs::data_->len_bounds(shapes.get_shape(0).length_);
		timer_mapping.stop();
		timer.finish();
		//const bool long_addressing_query = query_seqs::data_->raw_len() > (size_t)std::numeric_limits<uint32_t>::max();

		run_query_chunk(db_file, timer_mapping, total_timer, current_query_chunk, query_len_bounds, master_out);
	}

	timer.go("Closing the output file");
	timer_mapping.resume();
	master_out.finish();
	timer_mapping.stop();

	timer.go("Closing the database file");
	db_file.close();

	timer.finish();
	message_stream << "Total time = " << total_timer.getElapsedTimeInSec() << "s" << endl;
	verbose_stream << "Mapping time = " << timer_mapping.getElapsedTimeInSec() << "s" << endl;
	statistics.print();
}

void master_thread()
{
	Timer timer2, timer_mapping;
	timer2.start();

	align_mode = Align_mode(Align_mode::from_command(config.command));

	message_stream << "Temporary directory: " << Temp_file::get_temp_dir() << endl;

	task_timer timer ("Opening the database", 1);
	Database_file db_file;
	timer.finish();
	config.set_chunk_size(ref_header.block_size);
	verbose_stream << "Reference = " << config.database << endl;
	verbose_stream << "Sequences = " << ref_header.sequences << endl;
	verbose_stream << "Letters = " << ref_header.letters << endl;
	verbose_stream << "Block size = " << (size_t)(ref_header.block_size * 1e9) << endl;
	Config::set_option(config.db_size, ref_header.letters);

	master_thread(db_file, timer_mapping, timer2);
}

#endif /* MASTER_THREAD_H_ */
