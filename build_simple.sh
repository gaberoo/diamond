gcc -c -O3 -DNDEBUG src/blast/sm_blosum45.c src/blast/sm_blosum50.c src/blast/sm_blosum62.c src/blast/sm_blosum80.c src/blast/sm_blosum90.c src/blast/sm_pam30.c src/blast/sm_pam70.c src/blast/sm_pam250.c
g++ -DNDEBUG -O3 -mssse3 -static \
  sm*.o \
  src/run/main.cpp \
  src/basic/config.cpp \
  src/util/tinythread.cpp \
  src/util/compressed_stream.cpp \
  src/basic/score_matrix.cpp \
  src/blast/blast_filter.cpp \
  src/blast/blast_seg.cpp \
  src/data/queries.cpp \
  src/data/reference.cpp \
  src/data/seed_histogram.cpp \
  src/output/daa_record.cpp \
  src/search/search.cpp \
  src/util/command_line_parser.cpp \
  src/util/seq_file_format.cpp \
  src/util/util.cpp  \
  src/util/Timer.cpp \
  src/basic/basic.cpp \
  src/dp/floating_sw.cpp \
  src/align/align_sequence_anchored.cpp \
  src/align/align_sequence_simple.cpp \
  src/basic/hssp.cpp \
  src/dp/ungapped_align.cpp \
  src/run/tools.cpp \
  src/dp/greedy_align.cpp \
  src/run/benchmark.cpp \
  src/search/stage2.cpp \
  src/output/output_format.cpp \
-lz -lpthread -o diamond
