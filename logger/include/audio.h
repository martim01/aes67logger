#pragma once
#include <wx/event.h>
#include <queue>
#include "portaudio.h"
#include "kiss_fftr.h"
#include <list>
#include <wx/datetime.h>

class SoundFile;

struct freq_mag
{
    float dAmplitude[1025];
    std::list<int> lstPeaks;

    void WorkoutPeaks(double dLimit);


};

/** Class the reads in stereo audio from the sound card and compares the A and B leg to see if they sound the same. This class uses portaudio to read in the audio and pHash to do the comparison
**/
class Audio  : public wxEvtHandler
{
public:
    /** Constructor
    *   @param pHandler the wxEvtHandler to send the results of the comparison to
    *   @param nDevice the id of the sound card input
    **/
    Audio(wxEvtHandler* pHandler, unsigned int nDevice);

    ///< Destructor
    ~Audio();

    /** Initializes portaudio
    **/
    bool Init();

    unsigned int GetDevice() const
    {
        return m_nDevice;
    }


    void SetPeakCutoff(double ddB)
    {
        m_dPeakdB = ddB;
    }
    /** Function called by portaudio when it has received a buffer worth of audio. Checks whether enough audio has been collected and if so does the comparison
    *   @param pBuffer pointer to the buffer containing the audio samples
    *   @param nFrameCount the number of audio frames (2 x number of samples)
    **/
    void Callback(const float* pBuffer, size_t nFrameCount);

    void RecordAudio(bool bRecord);

    void SetGain(int nGain);

    freq_mag GetResult();
    void ClearResult();

    double GetLastPeak(int nType);

    enum {SAMPLE = 1, SEC=2, MIN=4, HOUR=8, DAY=16};

    void SaveSamples(const float* pBuffer, size_t nFrameCount);

private:

    /** Works out the offset between the A and B legs and starts a HashThread to do the actual comparison
    **/
    void Frequency();

    /** Attempts to open a portaudio stream on the given device to read in audio
    *   @param streamCallback function pointer to the callback function
    *   @return <i>bool</i> true on success
    **/
    bool OpenStream(PaStreamCallback *streamCallback);

    void CloseFile();

    int StorePeaks();

    wxEvtHandler* m_pManager;           ///< the wxEvtHandler that receives event messages from this class
    std::queue<float> m_qBuffer;      ///< vector containing the a-leg samples

    PaStream* m_pStream;                ///< pointer to the PaStream tha reads in the audio

    kiss_fft_scalar m_fft_inA[2048];
    kiss_fft_scalar m_fft_inB[2048];

    kiss_fft_cpx m_fft_out[1025];

    std::queue<freq_mag> m_qResult;

    int m_nDevice;
    double m_dGain;
    double m_dPeakdB;
    bool m_bRecord;


    SoundFile* m_pFile;
    wxDateTime m_dtFile;
    const PaDeviceInfo* m_pInfo;

    double m_dPeaks[24];
    double m_dPeakSec[60];
    double m_dPeakMin[60];
    double m_dPeakHour[24];
    std::list<double> m_lstPeakDay;

    unsigned int m_nPeakCount;
    unsigned int m_nPeakSecCount;
    unsigned int m_nPeakMinCount;
    unsigned int m_nPeakHourCount;

};

/** PaStreamCallback function - simply calls wmComparitor::Callback using userData to get the wmComparitor object
**/
int paCallback( const void *input, void *output, unsigned long frameCount, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *userData );
//int paCallbackB( const void *input, void *output, unsigned long frameCount, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *userData );


DECLARE_EXPORTED_EVENT_TYPE(WXEXPORT, wxEVT_FFT, -1)

