
ntr: *.cpp *.h keywords.gperf.gen.h 
	g++ -g *.cpp -o ntr

keywords.gperf.gen.h: keywords.gperf common.h
	gperf $< --output-file=$@

clean:
	rm ntr keywords.gperf.gen.h
