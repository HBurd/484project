SOURCE = phase_vocoder.cpp audio.cpp util.cpp dsp.cpp midi.cpp

phase_vocoder: $(SOURCE)
	g++  -o phase_vocoder $(SOURCE) -ggdb -ljack -lasound -lpthread
