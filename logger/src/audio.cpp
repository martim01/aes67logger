#include "audio.h"
#include <wx/log.h>
#include "portaudio.h"
#include <wx/msgdlg.h>
#include "soundfile.h"
#include <wx/datetime.h>

DEFINE_EVENT_TYPE(wxEVT_FFT)

using namespace std;



Audio::Audio(wxEvtHandler* pHandler, unsigned int nDevice) :
 m_pManager(pHandler),
 m_pStream(0),
 m_nDevice(nDevice),
 m_dGain(1),
 m_dPeakdB(-50),
 m_pFile(0),
 m_pInfo(Pa_GetDeviceInfo(nDevice)),
 m_nPeakCount(0),
 m_nPeakSecCount(0),
 m_nPeakMinCount(0),
 m_nPeakHourCount(0)
 {

 }

 bool Audio::Init()
 {

    PaError err = Pa_Initialize();
    if(err == paNoError)
    {
        if(OpenStream(paCallback))
        {
            return true;

        }
    }

    return false;

}
bool Audio::OpenStream(PaStreamCallback *streamCallback)
{
    PaStreamParameters inputParameters;
    inputParameters.channelCount = 2;
    inputParameters.device = m_nDevice;
    inputParameters.hostApiSpecificStreamInfo = NULL;
    inputParameters.sampleFormat = paFloat32;
    inputParameters.suggestedLatency = 0;
    inputParameters.hostApiSpecificStreamInfo = NULL;
    PaError err = Pa_OpenStream(&m_pStream, &inputParameters, 0, 48000, 2048, paNoFlag, streamCallback, reinterpret_cast<void*>(this) );
    if(err == paNoError)
    {
        err = Pa_StartStream(m_pStream);
        if(err == paNoError)
        {
            return true;
        }
    }
    return false;
}


Audio::~Audio()
{
    if(m_pStream)
    {
        Pa_AbortStream(m_pStream);
    }
    if(m_pFile)
    {
        CloseFile();
    }
    Pa_Terminate();

}

void Audio::Callback(const float* pBuffer, size_t nFrameCount)
{

    if(m_bRecord)
    {
        SaveSamples(pBuffer, nFrameCount);
    }

    double dMax(0);
    for(size_t i = 0; i < nFrameCount*2; i++)
    {
        double dValue = pBuffer[i]*m_dGain;
        m_qBuffer.push(dValue);
        if(dMax < dValue)
        {
            dMax = dValue;
        }
    }


    if(m_qBuffer.size() >= 4096)    //this is the size we want
    {
        for(size_t i = 0; i < 2048; i++)
        {
            m_fft_inA[i] = m_qBuffer.front();
            m_qBuffer.pop();

            m_fft_inB[i] = m_qBuffer.front();
            m_qBuffer.pop();
        }

        //Now do the check
        kiss_fftr_cfg cfg;
        if ((cfg = kiss_fftr_alloc(2048, 0/*is_inverse_fft*/, NULL, NULL)) != NULL)
        {
            kiss_fftr(cfg, m_fft_inA, m_fft_out);
            free(cfg);

            //now copy in to our FFT magnitude
            m_qResult.push(freq_mag());
            for(int i = 0; i < 1025; i++)
            {
                m_qResult.back().dAmplitude[i] = sqrt( (m_fft_out[i].r*m_fft_out[i].r) + (m_fft_out[i].i*m_fft_out[i].i));
            }

            m_qResult.back().WorkoutPeaks(m_dPeakdB);

            int nChanged = StorePeaks();


            //Pass the data back up...
            if(m_pManager)
            {
                wxCommandEvent event(wxEVT_FFT);
                event.SetInt(nChanged);
                wxPostEvent(m_pManager, event);
            }
        }
        else
        {
            free(cfg);
        }
    }
}

int Audio::StorePeaks()
{
    int nChanged(SAMPLE);

    m_nPeakCount++;
    if(m_nPeakCount == 24)
    {

        nChanged |= SEC;

        m_nPeakSecCount++;
        if(m_nPeakSecCount == 60)
        {


            nChanged |= MIN;

            m_nPeakMinCount++;
            if(m_nPeakMinCount == 60)
            {
                nChanged |= HOUR;

                m_nPeakHourCount++;
                if(m_nPeakHourCount == 24)
                {
                    nChanged |= DAY;

                    //work out the average over the last 24 hours and add to the days...
                    double dTotal = 0;
                    for(unsigned int i = 0; i < 24; i++)
                    {
                        dTotal += m_dPeakHour[i];
                    }
                    m_lstPeakDay.push_back(dTotal/24.0);
                    m_nPeakHourCount = 0;
                }

                double dTotal = 0;
                for(unsigned int i = 0; i < 60; i++)
                {
                    dTotal += m_dPeakMin[i];
                }
                m_dPeakHour[m_nPeakHourCount] = dTotal / 60.0;

                m_nPeakMinCount = 0;
            }

            double dTotal = 0;
            for(unsigned int i = 0; i < 60; i++)
            {
                dTotal += m_dPeakSec[i];
            }
            m_dPeakMin[m_nPeakMinCount] = dTotal / 60.0;

            wxLogDebug(wxT("Minute %d = %.2f"), m_nPeakMinCount, m_dPeakMin[m_nPeakMinCount]);

            m_nPeakSecCount = 0;
        }

        double dTotal = 0;
        for(unsigned int i = 0; i < 24; i++)
        {
            dTotal += m_dPeaks[i];
        }
        m_dPeakSec[m_nPeakSecCount] = dTotal / 24.0;

        m_nPeakCount = 0;
    }
    m_dPeaks[m_nPeakCount] = static_cast<double>(m_qResult.back().lstPeaks.size());

    return nChanged;
}

double Audio::GetLastPeak(int nType)
{
    double dReturn(0);
    switch(nType)
    {
        case SAMPLE:
            dReturn = m_dPeaks[m_nPeakCount];
            break;
        case SEC:
            dReturn = m_dPeakSec[m_nPeakSecCount];
            break;
        case MIN:
            dReturn = m_dPeakMin[m_nPeakMinCount];
            break;
        case HOUR:
            dReturn = m_dPeakHour[m_nPeakHourCount];
            break;
        case DAY:
            if(m_lstPeakDay.empty() == false)
            {
                dReturn = m_lstPeakDay.back();
            }
            break;
    }
    return dReturn;
}

void Audio::SetGain(int nGain)
{
    m_dGain = pow(2.0, static_cast<double>(nGain));
    wxLogDebug(wxT("Gain = %.2f"), m_dGain);
}


void Audio::SaveSamples(const float* pBuffer, size_t nFrameCount)
{
    if(m_pFile && (wxDateTime::Now()-m_dtFile).GetMinutes() > 10)
    {
        CloseFile();
    }


    if(!m_pFile)
    {
        m_dtFile = wxDateTime::Now();
        m_pFile = new SoundFile();
        m_pFile->OpenToWrite(wxString::Format(wxT("Dev_%03d_%s"), m_nDevice, m_dtFile.Format(wxT("%Y_%m_%dT%H_%M_%S.wav")).c_str()), m_pInfo->maxInputChannels, m_pInfo->defaultSampleRate, 16);
    }



    m_pFile->WriteAudio(pBuffer, nFrameCount*2);

}



int paCallback( const void *input, void *output, unsigned long frameCount, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *userData )
{
    if(userData && input)
    {
        Audio* pComp = reinterpret_cast<Audio*>(userData);
        pComp->Callback(reinterpret_cast<const float*>(input), frameCount);
    }
    return 0;
}


freq_mag Audio::GetResult()
{
    return m_qResult.front();
}

void Audio::ClearResult()
{
    m_qResult.pop();
}




void freq_mag::WorkoutPeaks(double dLimit)
{
    float dLast(0);
    bool bDirectionUp(false);
    wxLogDebug(wxT("Limit = %.2f"), dLimit);

    for(int i = 0; i < 1025; i++)
    {
        if(i != 0)
        {
            if(dAmplitude[i] < dLast)
            {
                if(bDirectionUp)
                {
                    bDirectionUp = false;
                    if(dLast > 1)  //let's ignore the noise floor
                    {
                        double dAmpl = 20*log10(dLast/1024);
                        //wxLogDebug(wxT("Ampl = %.2f"), dAmpl);
                        if(dAmpl > dLimit)
                        {
                            lstPeaks.push_back(i-1);
                        }
                    }
                }
            }
            else if(bDirectionUp == false)
            {
                bDirectionUp = true;
            }
        }
        dLast = dAmplitude[i];
    }
}


void Audio::RecordAudio(bool bRecord)
{
    if(m_bRecord && m_pFile)
    {
        CloseFile();
    }
    m_bRecord = bRecord;
}

void Audio::CloseFile()
{
    m_pFile->Close();
    delete m_pFile;
    m_pFile = 0;
}
