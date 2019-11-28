SOURCE = phase_vocoder.cpp audio.cpp util.cpp

phase_vocoder: $(SOURCE)
	g++ -o phase_vocoder $(SOURCE) -ljack
