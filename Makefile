SOURCE = phase_vocoder.cpp audio.cpp util.cpp dsp.cpp midi.cpp

phase_vocoder: $(SOURCE)
	g++ -o phase_vocoder $(SOURCE) kiss_fft.o -ggdb -ljack -lasound -lpthread

clean:
	rm phase_vocoder
