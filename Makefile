
ntr: *.cpp keywords.gperf.gen.h 
	g++ *.cpp -o ntr

keywords.gperf.gen.h: keywords.gperf common.h
	gperf $< --output-file=$@

clean:
	rm ntr keywords.gperf.gen.h
