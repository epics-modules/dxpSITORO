/***

==================================================================
* Copyright (C) 2016 Southern Innovation International Pty Ltd.  *
* All rights reserved.                                           *
*                                                                *
* Unauthorised copying, distribution, use, modification, reverse *
* engineering or disclosure is strictly prohibited.              *
*                                                                *
* This file, its contents and the  fact of its disclosure to you *
* is  Confidential  Information  and  Proprietary Information of *
* Southern   Innovation   International  Pty  Ltd  and  Southern *
* Innovation Trading Pty Ltd.   Unauthorised  disclosure of such *
* information is strictly prohibited.                            *
*                                                                *
* THIS SOURCE CODE AND  SOFTWARE IS PROVIDED "AS IS" WITHOUT ANY *
* REPRESENTATIONS  OR WARRANTIES OTHER  THAN THOSE WHICH MAY NOT *
* BE  EXCLUDED BY LAW.   IN NO EVENT  SHALL  SOUTHERN INNOVATION *
* INTERNATIONAL PTY LTD  OR  SOUTHERN INNOVATION TRADING PTY LTD *
* BE LIABLE  FOR ANY SPECIAL, DIRECT, INDIRECT  OR CONSEQUENTIAL *
* DAMAGES   HOWSOEVER  INCURRED  (INCLUDING  BY  NEGLIGENCE)  IN *
* CONNECTION WITH THE USE OF THIS SOURCE CODE AND SOFTWARE.      *
* WHERE SUCH LIABILITY  MAY ONLY BE LIMITED AT LAW, LIABILITY IS *
* LIMITED TO THE MAXIMUM EXTENT PERMITTED BY LAW.                *
==================================================================

***/

#ifndef SINCPLUSPLUS_H
#define SINCPLUSPLUS_H

//
// Sinc library wrapper for C++
//

#ifndef __cplusplus
#error This header is designed for C++ only.
#endif

#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <algorithm>
#include <thread>
#include <mutex>
#include "sinc.h"
#include "sinc_internal.h"

namespace SincProtocol
{
    typedef SiToro__Sinc__MessageType MessageType;
    typedef SiToro__Sinc__ErrorCode ErrorCode;


    //
    // An error code and a corresponding message.
    //

    class Error
    {
        SincError err_;

    public:
        Error()
        {
            err_.code = SI_TORO__SINC__ERROR_CODE__NO_ERROR;
            err_.msg[0] = 0;
        }

        SincError *getSincError() { return &err_; }

        ErrorCode getCode()  { return err_.code; }
        std::string getMsg() { return err_.msg; }
    };


    //
    // Oscilloscope plots.
    //

    class OscPlot
    {
        SincOscPlot plot_;

    public:
        OscPlot()
        {
            plot_.len = 0;
            plot_.data = nullptr;
            plot_.intData = nullptr;
            plot_.minRange = 0;
            plot_.maxRange = 0;
        }

        ~OscPlot()
        {
            if (plot_.data)
                free(plot_.data);

            if (plot_.intData)
                free(plot_.intData);
        }

        SincOscPlot *getSincOscPlot() { return &plot_; }
    };


    //
    // Calibration plots.
    //

    class CalibrationPlot
    {
        SincCalibrationPlot plot_;

    public:
        CalibrationPlot()
        {
            plot_.len = 0;
            plot_.x = nullptr;
            plot_.y = nullptr;
        }

        CalibrationPlot(std::vector<double> &x, std::vector<double> &y)
        {
            plot_.len = x.size();
            plot_.x = (double *) malloc(x.size() * sizeof(double));
            plot_.y = (double *) malloc(x.size() * sizeof(double));
            std::copy_n(x.begin(), x.size(), plot_.x);
            std::copy_n(y.begin(), y.size(), plot_.y);
        }

        CalibrationPlot(const CalibrationPlot &p)
        {
            plot_.len = p.plot_.len;
            plot_.x = (double *) malloc(plot_.len * sizeof(double));
            plot_.y = (double *) malloc(plot_.len * sizeof(double));
            std::copy_n(p.plot_.x, plot_.len, plot_.x);
            std::copy_n(p.plot_.y, plot_.len, plot_.y);
        }

        ~CalibrationPlot()
        {
            if (plot_.x)
                free(plot_.x);

            if (plot_.y)
                free(plot_.y);
        }

        SincCalibrationPlot *getPlot() { return &plot_; }
        const SincCalibrationPlot *getPlot() const { return &plot_; }
    };


    //
    // Histogram plots.
    //

    class HistogramPlot
    {
        SincHistogram plot_;

    private:
        HistogramPlot(const HistogramPlot &h);

    public:
        HistogramPlot()
        {
            plot_.len = 0;
            plot_.data = nullptr;
        }

        ~HistogramPlot()
        {
            if (plot_.data)
                free(plot_.data);
        }

        // Move constructor and assignment operator for efficiency.
        HistogramPlot(HistogramPlot &&h)
        {
            plot_.len = h.plot_.len;
            plot_.data = h.plot_.data;
            h.plot_.len = 0;
            h.plot_.data = nullptr;
        }

        HistogramPlot& operator=(HistogramPlot &&h)
        {
            if (this == &h)
                return *this;

            if (plot_.data)
                free(plot_.data);

            plot_.len = h.plot_.len;
            plot_.data = h.plot_.data;
            h.plot_.len = 0;
            h.plot_.data = nullptr;

            return *this;
        }

        SincHistogram *getSincHistogram() { return &plot_; }
    };


    //
    // A key/value used to set or get parameter attributes.
    //

    class KeyValue
    {
        SiToro__Sinc__KeyValue kv_;
        std::string key_;
        std::string str_;

    public:
        KeyValue() { si_toro__sinc__key_value__init(&kv_); }
        KeyValue(const std::string &key, int64_t intVal, int channelId = -1) { init(channelId, key); setParamType(SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__INT_TYPE); kv_.intval = intVal; kv_.has_intval = true; }
        KeyValue(const std::string &key, double floatVal, int channelId = -1) { init(channelId, key); setParamType(SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__FLOAT_TYPE); kv_.floatval = floatVal; kv_.has_floatval = true; }
        KeyValue(const std::string &key, bool boolVal, int channelId = -1) { init(channelId, key); setParamType(SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__BOOL_TYPE); kv_.boolval = boolVal; kv_.has_boolval = true; }
        KeyValue(const std::string &key, const std::string &strVal, int channelId = -1) { init(channelId, key); setParamType(SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__STRING_TYPE); str_ = strVal; kv_.strval = const_cast<char *>(str_.c_str()); }
        KeyValue(const std::string &key, const std::string &optionVal, bool isOption, int channelId = -1) { init(channelId, key); str_ = optionVal; if (isOption) { setParamType(SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__OPTION_TYPE); kv_.optionval = const_cast<char *>(str_.c_str()); } else { setParamType(SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__STRING_TYPE); kv_.strval = const_cast<char *>(str_.c_str()); } }

        // Copy constructor.
        KeyValue(const KeyValue &v)
        {
            copy(v.kv_);
        }

        KeyValue(const SiToro__Sinc__KeyValue &v)
        {
            copy(v);
        }

        ~KeyValue() {}

        void copy(const SiToro__Sinc__KeyValue &v)
        {
            si_toro__sinc__key_value__init(&kv_);

            if (v.has_channelid)
                setChannelId(v.channelid);

            setKey(v.key);
            if (v.has_intval)
            {
                kv_.has_intval = true;
                kv_.intval = v.intval;
            }

            if (v.has_floatval)
            {
                kv_.has_floatval = true;
                kv_.floatval = v.floatval;
            }

            if (v.has_boolval)
            {
                kv_.has_boolval = true;
                kv_.boolval = v.boolval;
            }

            if (v.strval)
            {
                str_ = v.strval;
                kv_.strval = const_cast<char *>(str_.data());
            }

            if (v.optionval)
            {
                str_ = v.optionval;
                kv_.optionval = const_cast<char *>(str_.data());
            }

            if (v.has_paramtype)
            {
                kv_.has_paramtype = true;
                kv_.paramtype = v.paramtype;
            }
        }

        SiToro__Sinc__KeyValue *getSincKeyValue() { return &kv_; }
        const SiToro__Sinc__KeyValue *getSincKeyValue() const { return &kv_; }

        void init(int channelId, const std::string &key) { si_toro__sinc__key_value__init(&kv_); setChannelId(channelId); setKey(key); }
        void setChannelId(int channelId) { if (channelId >= 0) { kv_.has_channelid = true; kv_.channelid = channelId; } }
        void setKey(const std::string &key) { key_ = key; kv_.key = const_cast<char *>(key_.data()); }
        void setParamType(SiToro__Sinc__KeyValue__ParamType paramType) { kv_.has_paramtype = true; kv_.paramtype = paramType; }

        SiToro__Sinc__KeyValue__ParamType getParamType() const { if (kv_.has_paramtype) return kv_.paramtype; else return SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__NO_TYPE; }
        int64_t     getInt()    const { return kv_.intval; }
        double      getFloat()  const { return kv_.floatval; }
        bool        getBool()   const { return kv_.boolval; }
        std::string getString() const { return kv_.strval; }
        std::string getOption() const { return kv_.optionval; }

        void copyTo(SiToro__Sinc__KeyValue &dest) const
        {
            si_toro__sinc__key_value__init(&dest);
            dest.key = kv_.key;
            dest.has_channelid = kv_.has_channelid;
            dest.channelid = kv_.channelid;
            dest.has_intval = kv_.has_intval;
            dest.intval = kv_.intval;
            dest.has_floatval = kv_.has_floatval;
            dest.floatval = kv_.floatval;
            dest.has_boolval = kv_.has_boolval;
            dest.boolval = kv_.boolval;
            dest.strval = kv_.strval;
            dest.optionval = kv_.optionval;
            dest.has_paramtype = kv_.has_paramtype;
            dest.paramtype = kv_.paramtype;
        }
    };


    //
    // Wrappers for various types of response.
    //

    class SuccessResponse
    {
        int fromChannelId_;
        SiToro__Sinc__SuccessResponse *resp_;

    public:
        SuccessResponse() : fromChannelId_(0), resp_(nullptr) {}
        ~SuccessResponse() { if (resp_) si_toro__sinc__success_response__free_unpacked(resp_, NULL); }

        SiToro__Sinc__SuccessResponse *getSincSuccessResponse() { return resp_; }
        void set(int fromChannelId, SiToro__Sinc__SuccessResponse *r) { fromChannelId_ = fromChannelId; if (resp_) si_toro__sinc__success_response__free_unpacked(resp_, NULL); resp_ = r; }

        int getChannelId() const { return fromChannelId_; }
    };

    class GetParamResponse
    {
        int fromChannelId_;
        SiToro__Sinc__GetParamResponse *resp_;

    public:
        GetParamResponse() : fromChannelId_(0), resp_(nullptr) {}
        ~GetParamResponse() { if (resp_) si_toro__sinc__get_param_response__free_unpacked(resp_, NULL); }

        void set(int fromChannelId, SiToro__Sinc__GetParamResponse *r) { fromChannelId_ = fromChannelId; if (resp_) si_toro__sinc__get_param_response__free_unpacked(resp_, NULL); resp_ = r; }

        int getChannelId() const { return fromChannelId_; }
        int getNumParams() const { return resp_->n_results; }
        const SiToro__Sinc__KeyValue *getParam(int id) const { return resp_->results[id]; }
    };

    class ParamUpdatedResponse
    {
        int fromChannelId_;
        SiToro__Sinc__ParamUpdatedResponse *resp_;

    public:
        ParamUpdatedResponse() : fromChannelId_(0), resp_(nullptr) {}
        ~ParamUpdatedResponse() { if (resp_) si_toro__sinc__param_updated_response__free_unpacked(resp_, NULL); }

        void set(int fromChannelId, SiToro__Sinc__ParamUpdatedResponse *r) { fromChannelId_ = fromChannelId; if (resp_) si_toro__sinc__param_updated_response__free_unpacked(resp_, NULL); resp_ = r; }
    };

    class CalibrationProgressResponse
    {
        int fromChannelId_;
        SiToro__Sinc__CalibrationProgressResponse *resp_;
        double progress_;
        int complete_;
        std::string stage_;

    public:
        CalibrationProgressResponse() : fromChannelId_(0), resp_(nullptr), progress_(0), complete_(0) {}
        ~CalibrationProgressResponse() { if (resp_) si_toro__sinc__calibration_progress_response__free_unpacked(resp_, NULL); }

        void set(int fromChannelId, SiToro__Sinc__CalibrationProgressResponse *r, double progress, int complete, const char *stage) { fromChannelId_ = fromChannelId; progress_ = progress; complete_ = complete; stage_ = stage; if (resp_) si_toro__sinc__calibration_progress_response__free_unpacked(resp_, NULL); resp_ = r; }
        int getComplete() {return complete_;}
        double getProgress() {return progress_;}
        std::string getStage() {return stage_;}
        int getFromChannelId() {return fromChannelId_;}
    };

    class CalibrationInfo
    {
        int fromChannelId_;
        std::string calibData_;
        CalibrationPlot example_;
        CalibrationPlot model_;
        CalibrationPlot final_;
        SiToro__Sinc__GetCalibrationResponse *resp_;

    public:
        CalibrationInfo(): fromChannelId_(0), resp_(nullptr) {}
        CalibrationInfo(int channelId, std::string calibData, const CalibrationPlot &example, const CalibrationPlot &model, const CalibrationPlot &final) :
            example_(example),
            model_(model),
            final_(final),
            resp_(nullptr)
        {
            fromChannelId_ = channelId;
            calibData_ = calibData;
        }
        ~CalibrationInfo() { if (resp_) si_toro__sinc__get_calibration_response__free_unpacked(resp_, NULL); }

        void set(int fromChannelId, SiToro__Sinc__GetCalibrationResponse *r, const std::string &calData) { fromChannelId_ = fromChannelId; calibData_ = calData; if (resp_) si_toro__sinc__get_calibration_response__free_unpacked(resp_, NULL); resp_ = r; }

        int getChannelId() const { return fromChannelId_; }
        SiToro__Sinc__GetCalibrationResponse *&getResponse() { return resp_; }
        CalibrationPlot &getExample() { return example_; }
        CalibrationPlot &getModel() { return model_; }
        CalibrationPlot &getFinal() { return final_; }
        const CalibrationPlot &getExample() const { return example_; }
        const CalibrationPlot &getModel() const { return model_; }
        const CalibrationPlot &getFinal() const { return final_; }

        const std::string &getCalibData() const { return calibData_; }
    };

    class ParamDetails
    {
        int fromChannelId_;
        SiToro__Sinc__ListParamDetailsResponse *resp_;

    public:
        ParamDetails() : fromChannelId_(0), resp_(nullptr) {}
        ~ParamDetails() { if (resp_) si_toro__sinc__list_param_details_response__free_unpacked(resp_, NULL); }

        void set(int fromChannelId, SiToro__Sinc__ListParamDetailsResponse *r) { fromChannelId_ = fromChannelId; if (resp_) si_toro__sinc__list_param_details_response__free_unpacked(resp_, NULL); resp_ = r; }

        int getChannelId() const { return fromChannelId_; }
        int getNumParams() const { return resp_->n_paramdetails; }
        SiToro__Sinc__ParamDetails *getParam(int id) const { return resp_->paramdetails[id]; }
    };

    class OscilloscopeData
    {
        int fromChannelId_;
        uint64_t dataSetId_;
        OscPlot resetBlanked_;
        OscPlot rawCurve_;

    public:
        OscilloscopeData() : fromChannelId_(0), dataSetId_(0) {}

        void set(int fromChannelId, uint64_t dataSetId) { fromChannelId_ = fromChannelId; dataSetId_ = dataSetId; }

        int getChannelId() const { return fromChannelId_; }
        OscPlot &getResetBlanked() { return resetBlanked_; }
        OscPlot &getRaw() { return rawCurve_; }
    };

    class HistogramData
    {
        int fromChannelId_;
        HistogramPlot accepted_;
        HistogramPlot rejected_;
        SincHistogramCountStats stats_;

    public:
        HistogramData() : fromChannelId_(0) { stats_ = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, SI_TORO__SINC__HISTOGRAM_TRIGGER__REFRESH_UPDATE, 0, nullptr}; }
        ~HistogramData() { if (stats_.intensityData) free(stats_.intensityData); }

        void set(int fromChannelId)
        {
            fromChannelId_ = fromChannelId;
        }

        int getChannelId() const { return fromChannelId_; }
        HistogramPlot &getAccepted() { return accepted_; }
        HistogramPlot &getRejected() { return rejected_; }
        SincHistogramCountStats &getStats() { return stats_; }
    };

    class ListModeData
    {
        int fromChannelId_;
        uint64_t dataSetId_;
        uint8_t *data_;
        int dataLen_;

    public:
        ListModeData() : fromChannelId_(0), dataSetId_(0), data_(nullptr), dataLen_(0) {}
        ~ListModeData() { if (data_) free(data_); }

        void set(int fromChannelId, uint64_t dataSetId, uint8_t *data, int dataLen) { if (data_) free(data_); fromChannelId_ = fromChannelId; dataSetId_ = dataSetId; data_ = data; dataLen_ = dataLen; }

        int getChannelId() const { return fromChannelId_; }
    };

    class AsynchronousErrorResponse
    {
        int fromChannelId_;
        SiToro__Sinc__AsynchronousErrorResponse *resp_;

    public:
        AsynchronousErrorResponse() : fromChannelId_(0), resp_(nullptr) {}
        ~AsynchronousErrorResponse() { if (resp_) si_toro__sinc__asynchronous_error_response__free_unpacked(resp_, NULL); }

        void set(int fromChannelId, SiToro__Sinc__AsynchronousErrorResponse *r) { fromChannelId_ = fromChannelId; if (resp_) si_toro__sinc__asynchronous_error_response__free_unpacked(resp_, NULL); resp_ = r; }

        int getChannelId() const { return fromChannelId_; }
    };

    class DownloadCrashDump
    {
        SiToro__Sinc__DownloadCrashDumpResponse *resp_;

    public:
        DownloadCrashDump() : resp_(nullptr) {}
        ~DownloadCrashDump() { if (resp_) si_toro__sinc__download_crash_dump_response__free_unpacked(resp_, NULL); }

        void set(SiToro__Sinc__DownloadCrashDumpResponse *r) { if (resp_) si_toro__sinc__download_crash_dump_response__free_unpacked(resp_, NULL); resp_ = r; }

        SiToro__Sinc__DownloadCrashDumpResponse getDownloadCrashDumpResponse() const { return *resp_; }
    };

    //
    // A buffer of encoded data.
    //

    class Buffer
    {
    protected:
        uint8_t    pad_[256*1024];
        SincBuffer buf_;

    public:
        Buffer() :
            pad_(""),
            buf_(SINC_BUFFER_INIT(pad_))
        {
        }

        ~Buffer()
        {
            SINC_BUFFER_CLEAR(&buf_);
        }


        // Accessors.
        SincBuffer &getSincBuffer() { return buf_; }
        const SincBuffer &getSincBuffer() const { return buf_; }


        //
        // NAME:        getNextPacketFromBuffer
        // ACTION:      Gets the next packet in the read buffer and de-encapsulates it.
        //              The resulting packet can be handed directly to the appropriate SincDecodeXXX()
        //              function.
        //              Use this function to read the next message from a buffer. If you want to read
        //              from the input stream use SincReadMessage() instead.
        // PARAMETERS:  SiToro__Sinc__MessageType &packetType - the found packet type is placed here.
        //              SincBuffer &packetBuf - where to put the resultant de-encapsulated packet.
        //                                      If NULL it won't make a buffer or consume data (this is useful
        //                                      for peeking ahead for the packet type).
        // RETURNS:     true if a complete packet was found, false otherwise.
        //

        bool getNextPacketFromBuffer(SiToro__Sinc__MessageType &packetType, SincBuffer &packetBuf)
        {
            int packetFound = false;
            SincGetNextPacketFromBuffer(&buf_, &packetType, &packetBuf, &packetFound);
            return packetFound;
        }


        //
        // Various packet decoding functions.
        //

        bool decodeSuccessResponse(SincError &err, SuccessResponse &resp)
        {
            int fromChannelId = 0;
            SiToro__Sinc__SuccessResponse *r = nullptr;
            bool ok = SincDecodeSuccessResponse(&err, &buf_, &r, &fromChannelId);
            resp.set(fromChannelId, r);
            return ok;
        }

        bool decodeGetParamResponse(SincError &err, GetParamResponse &resp)
        {
            int fromChannelId = 0;
            SiToro__Sinc__GetParamResponse *r = nullptr;
            bool ok = SincDecodeGetParamResponse(&err, &buf_, &r, &fromChannelId);
            resp.set(fromChannelId, r);
            return ok;
        }

        bool decodeParamUpdatedResponse(SincError &err, ParamUpdatedResponse &resp)
        {
            int fromChannelId = 0;
            SiToro__Sinc__ParamUpdatedResponse *r = nullptr;
            bool ok = SincDecodeParamUpdatedResponse(&err, &buf_, &r, &fromChannelId);
            resp.set(fromChannelId, r);
            return ok;
        }

        bool decodeCalibrationProgressResponse(SincError &err, CalibrationProgressResponse &resp)
        {
            int fromChannelId = 0;
            SiToro__Sinc__CalibrationProgressResponse *r = nullptr;
            double progress = 0.0;
            int complete = 0;
            char *stage = nullptr;

            bool ok = SincDecodeCalibrationProgressResponse(&err, &buf_, &r, &progress, &complete, &stage, &fromChannelId);
            resp.set(fromChannelId, r, progress, complete, stage ? stage : "");
            if (stage)
                free(stage);

            return ok;
        }

        bool decodeGetCalibrationResponse(SincError &err, CalibrationInfo &resp)
        {
            int fromChannelId;
            SiToro__Sinc__GetCalibrationResponse *r = nullptr;
            SincCalibrationData calData;
            std::string calDataStr;

            calData.data = nullptr;
            calData.len = 0;

            bool ok = SincDecodeGetCalibrationResponse(&err, &buf_, &r, &fromChannelId, &calData, resp.getExample().getPlot(), resp.getModel().getPlot(), resp.getFinal().getPlot());
            if (ok && calData.data)
            {
                calDataStr = std::string((char *)calData.data, calData.len);
            }

            resp.set(fromChannelId, r, calDataStr);
            if (calData.data)
                free(calData.data);

            return ok;
        }

        bool decodeCalculateDcOffsetResponse(SincError &err, int &fromChannelId, double &dcOffset)
        {
            return SincDecodeCalculateDCOffsetResponse(&err, &buf_, nullptr, &dcOffset, &fromChannelId);
        }

        bool decodeListParamDetailsResponse(SincError &err, ParamDetails &resp)
        {
            int fromChannelId = 0;
            SiToro__Sinc__ListParamDetailsResponse *r = nullptr;

            bool ok = SincDecodeListParamDetailsResponse(&err, &buf_, &r, &fromChannelId);
            resp.set(fromChannelId, r);

            return ok;
        }

        bool decodeOscilloscopeDataResponse(SincError &err, OscilloscopeData &resp)
        {
            int fromChannelId = 0;
            uint64_t dataSetId = 0;

            bool ok = SincDecodeOscilloscopeDataResponse(&err, &buf_, &fromChannelId, &dataSetId, resp.getResetBlanked().getSincOscPlot(), resp.getRaw().getSincOscPlot());
            resp.set(fromChannelId, dataSetId);

            return ok;
        }

        bool decodeHistogramDataResponse(SincError &err, HistogramData &resp)
        {
            int fromChannelId = 0;

            bool ok = SincDecodeHistogramDataResponse(&err, &buf_, &fromChannelId, resp.getAccepted().getSincHistogram(), resp.getRejected().getSincHistogram(), &resp.getStats());
            resp.set(fromChannelId);

            return ok;
        }

        bool decodeHistogramDatagramResponse(SincError &err, HistogramData &resp)
        {
            int fromChannelId = 0;

            bool ok = SincDecodeHistogramDatagramResponse(&err, &buf_, &fromChannelId, resp.getAccepted().getSincHistogram(), resp.getRejected().getSincHistogram(), &resp.getStats());
            resp.set(fromChannelId);

            return ok;
        }

        bool decodeListModeDataResponse(SincError &err, ListModeData &resp)
        {
            int fromChannelId = 0;
            uint64_t dataSetId = 0;
            uint8_t *data = nullptr;
            int dataLen = 0;

            bool ok = SincDecodeListModeDataResponse(&err, &buf_, &fromChannelId, &data, &dataLen, &dataSetId);
            resp.set(fromChannelId, dataSetId, data, dataLen);

            return ok;
        }

        bool decodeAsynchronousErrorResponse(SincError &err, AsynchronousErrorResponse &resp)
        {
            int fromChannelId = 0;
            SiToro__Sinc__AsynchronousErrorResponse *r = nullptr;

            bool ok = SincDecodeAsynchronousErrorResponse(&err, &buf_, &r, &fromChannelId);
            resp.set(fromChannelId, r);

            return ok;
        }

        bool decodeSoftwareUpdateCompleteResponse(SincError &err)
        {
            return SincDecodeSoftwareUpdateCompleteResponse(&err, &buf_);
        }

        bool decodeDownloadCrashDumpResponse(SincError & /*err*/, DownloadCrashDump &/*resp*/)
        {
#if 0
            bool ok = SincDecodeDownloadCrashDumpResponse(&err, &buf_, &r);
            resp.set(r);
            return ok;
#else
            return false;
#endif
        }


        //
        // Various packet encoding functions which can be used stand-alone.
        //(SincBuffer *buf, int *channelIds, const char **names, int numNames)

        void encodePing(int showOnConsole) { SincEncodePing(&buf_, showOnConsole); }
        void encodeGetParam(int channelId, const std::string &name) { SincEncodeGetParam(&buf_, channelId, const_cast<char *>(name.c_str())); }
        bool encodeGetParams(std::vector<std::pair<int, std::string>> chanKeys)
        {
            std::vector<const char *> names(chanKeys.size());
            std::vector<int> channIds(chanKeys.size());
            for (size_t i = 0; i < chanKeys.size(); i++)
            {
                names[i] = chanKeys[i].second.c_str();
                channIds[i] = chanKeys[i].first;
            }

            return SincEncodeGetParams(&buf_, channIds.data(), names.data(), names.size());
        }
        void encodeSetParam(int channelId, const KeyValue &param) { SincEncodeSetParam(&buf_, channelId, const_cast<SiToro__Sinc__KeyValue *>(param.getSincKeyValue())); }
        bool encodeSetParams(int channelId, const std::vector<KeyValue> &params)
        {
            std::vector<SiToro__Sinc__KeyValue> vec(params.size());
            for (size_t i = 0; i < params.size(); i++)
            {
                params[i].copyTo(vec[i]);
            }

            return SincEncodeSetParams(&buf_, channelId, vec.data(), vec.size());
        }

        void encodeStartCalibration(int channelId) { SincEncodeStartCalibration(&buf_, channelId); }
        void encodeGetCalibration(int channelId) { SincEncodeGetCalibration(&buf_, channelId); }
        void encodeSetCalibration(int channelId, const CalibrationInfo &calInfo)
        {
            SincCalibrationData calData;
            calData.len = calInfo.getCalibData().length();
            calData.data = (uint8_t *)calInfo.getCalibData().data();
            SincEncodeSetCalibration(&buf_, channelId, &calData, const_cast<SincCalibrationPlot *>(calInfo.getExample().getPlot()), const_cast<SincCalibrationPlot *>(calInfo.getModel().getPlot()), const_cast<SincCalibrationPlot *>(calInfo.getFinal().getPlot()));
        }

        void encodeCalculateDcOffset(int channelId) { SincEncodeCalculateDcOffset(&buf_, channelId); }
        void encodeStartOscilloscope(int channelId) { SincEncodeStartOscilloscope(&buf_, channelId); }
        void encodeStartHistogram(int channelId) { SincEncodeStartHistogram(&buf_, channelId); }
        void encodeClearHistogramData(int channelId) { SincEncodeClearHistogramData(&buf_, channelId); }
        void encodeStartListMode(int channelId) { SincEncodeStartListMode(&buf_, channelId); }
        void encodeStop(int channelId, bool skip = false) { SincEncodeStop(&buf_, channelId, skip); }
        void encodeListParamDetails(int channelId) { SincEncodeListParamDetails(&buf_, channelId, nullptr); }
        void encodeRestart() { SincEncodeRestart(&buf_); }
        void encodeResetSpatialSystem() { SincEncodeResetSpatialSystem(&buf_); }
        void encodeSoftwareUpdate(const std::string &appImage, const std::string &appChecksum, const std::string &fpgaImage, const std::string &fpgaChecksum, bool autoRestart) { SincEncodeSoftwareUpdate(&buf_, reinterpret_cast<const uint8_t *>(appImage.data()), appImage.size(), appChecksum.c_str(), reinterpret_cast<const uint8_t *>(fpgaImage.data()), fpgaImage.size(), fpgaChecksum.c_str(), nullptr, 0, autoRestart); }
        void encodeSaveConfiguration() { SincEncodeSaveConfiguration(&buf_); }
        void encodeDeleteSavedConfiguration() { SincEncodeDeleteSavedConfiguration(&buf_); }
        void encodeMonitorChannels(const std::vector<int> &channels) { SincEncodeMonitorChannels(&buf_, channels.data(), channels.size()); }
        void encodeProbeDatagram() { SincEncodeProbeDatagram(&buf_); }
        void encodeDownloadCrashDump() { SincEncodeDownloadCrashDump(&buf_); }
    };


    //
    // The central SINC class. This is used to connect to devices and communicate with them.
    //

    class Sinc
    {
    protected:
        // The host/port to connect to.
        std::string host_;
        int port_;

        // The C Sinc structure.
        ::Sinc sinc_;

        // A mutex to prevent simultanously sending commands which would violate the protocol.
        std::mutex commandMutex_;

    public:
        Sinc(std::string host, int port = SINC_PORT) :
            host_(host),
            port_(port)
        {
            // Handle the case where the host is expressed as "host:port". Overrides the optional integer port.
            auto colonPos = host.find(':');
            if (colonPos != std::string::npos)
            {
                auto portStr = host.substr(colonPos+1);
                port_ = std::atoi(portStr.c_str());
                host_ = host.substr(0, colonPos);
            }

            SincInit(&sinc_);
        }

        ~Sinc()
        {
            SincCleanup(&sinc_);
        }


        //
        // NAME:        setTimeout
        // ACTION:      Sets a timeout for following commands.
        // PARAMETERS:  int timeout - timeout in milliseconds. -1 for no timeout.
        //

        void setTimeout(int timeoutMs)
        {
            SincSetTimeout(&sinc_, timeoutMs);
        }


        int  getTimeout() const { return sinc_.timeout; }


        //
        // NAME:        getFd
        // ACTION:      Gets the file descriptor of this SINC connection.
        //

        int  getFd() const
        {
            return sinc_.fd;
        }


        //
        // NAME:        getCommandMutex
        // ACTION:      A mutex used when sending a command which requires a response.
        //              Use a std::lock_guard to lock this mutex when using the requestXXX()
        //              methods.
        //

        std::mutex &getCommandMutex()
        {
            return commandMutex_;
        }


        //
        // NAME:        connect
        // ACTION:      Connects a Sinc channel to a device on the given host and port.
        // RETURNS:     true on success, false otherwise. On failure use currentErrorCode() and
        //                  currentErrorMessage() to get the error status.
        //

        bool connect()
        {
            std::lock_guard<std::mutex> locker(commandMutex_);
            return SincConnect(&sinc_, host_.c_str(), port_);
        }


        //
        // NAME:        disconnect
        // ACTION:      Disconnects a Sinc channel from whatever it's connected to.
        // RETURNS:     true on success, false otherwise. On failure use currentErrorCode() and
        //                  currentErrorMessage() to get the error status.
        //

        bool disconnect()
        {
            std::lock_guard<std::mutex> locker(commandMutex_);
            return SincDisconnect(&sinc_);
        }



        //
        // NAME:        isConnected
        // ACTION:      Returns the connected/disconnected state of the channel.
        // RETURNS:     int - true if connected, false otherwise.
        //

        bool isConnected() { return SincIsConnected(&sinc_); }


        //
        // Commands which wait for a response. These can be used stand-alone.
        //

        bool doPing(int showOnConsole)                                { std::lock_guard<std::mutex> locker(commandMutex_); if (!requestPing(showOnConsole)) return false; return waitSuccess(); }
        bool doGetParam(int channelId, const std::string &name, GetParamResponse &resp)  { std::lock_guard<std::mutex> locker(commandMutex_); if (!requestGetParam(channelId, name)) return false; return waitGetParamResponse(resp); }
        bool doGetParams(std::vector<std::pair<int, std::string>> chanKeys, GetParamResponse &resp)  { std::lock_guard<std::mutex> locker(commandMutex_); if (!requestGetParams(chanKeys)) return false; return waitGetParamResponse(resp); }
        bool doSetParam(int channelId, const KeyValue &param)         { std::lock_guard<std::mutex> locker(commandMutex_); if (!requestSetParam(channelId, param)) return false; return waitSuccess(); }
        bool doSetParams(int channelId, const std::vector<KeyValue> &params) { std::lock_guard<std::mutex> locker(commandMutex_); if (!requestSetParams(channelId, params)) return false; return waitSuccess(); }
        bool doCalibrate(int channelId)
        {
            std::lock_guard<std::mutex> locker(commandMutex_);
            if (!requestStartCalibration(channelId))
                return false;

            if (!waitCalibrationComplete(channelId))
                return false;

            return true;
        }

        bool doStartCalibration(int channelId)                        { std::lock_guard<std::mutex> locker(commandMutex_); if (!requestStartCalibration(channelId)) return false; return waitSuccess(); }
        bool doGetCalibration(int channelId, CalibrationInfo &calInfo) { std::lock_guard<std::mutex> locker(commandMutex_); if (!requestGetCalibration(channelId)) return false; return waitGetCalibrationResponse(calInfo); }
        bool doSetCalibration(int channelId, const CalibrationInfo &calInfo) { std::lock_guard<std::mutex> locker(commandMutex_); if (!requestSetCalibration(channelId, calInfo)) return false; return waitSuccess(); }
        bool doCalculateDcOffset(int channelId, int fromChannelId, double &offset) { std::lock_guard<std::mutex> locker(commandMutex_); if (!requestCalculateDcOffset(channelId)) return false; if (!waitSuccess()) return false; return waitCalculateDcOffsetResponse(fromChannelId, offset); }
        bool doStartOscilloscope(int channelId)                       { std::lock_guard<std::mutex> locker(commandMutex_); if (!requestStartOscilloscope(channelId)) return false; return waitSuccess(); }
        bool doStartHistogram(int channelId)                          { std::lock_guard<std::mutex> locker(commandMutex_); if (!requestStartHistogram(channelId)) return false; return waitSuccess(); }
        bool doClearHistogramData(int channelId)                      { std::lock_guard<std::mutex> locker(commandMutex_); if (!requestClearHistogramData(channelId)) return false; return waitSuccess(); }
        bool doStartListMode(int channelId)                           { std::lock_guard<std::mutex> locker(commandMutex_); if (!requestStartListMode(channelId)) return false; return waitSuccess(); }
        bool doStop(int channelId, bool skip = false)                 { std::lock_guard<std::mutex> locker(commandMutex_); if (!requestStop(channelId, skip)) return false; return waitSuccess(); }
        bool doListParamDetails(int channelId, ParamDetails &details) { std::lock_guard<std::mutex> locker(commandMutex_); if (!requestListParamDetails(channelId)) return false; return waitListParamDetailsResponse(details); }
        bool doRestart()                                              { std::lock_guard<std::mutex> locker(commandMutex_); if (!requestRestart()) return false; return waitSuccess(); }
        bool doResetSpatialSystem()                                   { std::lock_guard<std::mutex> locker(commandMutex_); if (!requestResetSpatialSystem()) return false; return waitSuccess(); }
        bool doSoftwareUpdate(const std::string &appImage, const std::string &appChecksum, const std::string &fpgaImage, const std::string &fpgaChecksum, bool autoRestart) { std::lock_guard<std::mutex> locker(commandMutex_); if (!requestSoftwareUpdate(appImage, appChecksum, fpgaImage, fpgaChecksum, autoRestart)) return false; return waitSuccess(); }
        bool doSaveConfiguration()                                    { std::lock_guard<std::mutex> locker(commandMutex_); if (!requestSaveConfiguration()) return false; return waitSuccess(); }
        bool doDeleteSavedConfiguration()                             { std::lock_guard<std::mutex> locker(commandMutex_); if (!requestDeleteSavedConfiguration()) return false; return waitSuccess(); }
        bool doMonitorChannels(const std::vector<int> &channels)      { std::lock_guard<std::mutex> locker(commandMutex_); if (!requestMonitorChannels(channels)) return false; return waitSuccess(); }
        bool doProbeDatagram()                                        { std::lock_guard<std::mutex> locker(commandMutex_); if (!requestProbeDatagram()) return false; return waitSuccess(); }
        bool doInitDatagramComms()                                    { std::lock_guard<std::mutex> locker(commandMutex_); if (!SincInitDatagramComms(&sinc_)) return false; return waitSuccess(); }
        bool doDownloadCrashDump(DownloadCrashDump &resp)             { std::lock_guard<std::mutex> locker(commandMutex_); if (!requestDownloadCrashDump()) return false; return waitDownloadCrashDumpResponse(resp); }


        //
        // NAME:        getErrorCode / getReadErrorCode / getWriteErrorCode
        // ACTION:      Get the most recent error code. getErrorCode() gets the most
        //              recent error code. getReadErrorCode() gets the most recent read error.
        //              getWriteErrorCode() gets the most recent write error.
        // PARAMETERS:  Sinc *sc - the sinc connection.
        //

        ErrorCode getErrorCode() const        { return sinc_.err->code; }
        ErrorCode getReadErrorCode() const    { return sinc_.readErr.code; }
        ErrorCode getWriteErrorCode() const   { return sinc_.writeErr.code; }


        //
        // NAME:        getErrorMessage / getReadErrorMessage / getWriteErrorMessage
        // ACTION:      Get the most recent error message in alphanumeric form. getErrorMessage()
        //              gets the most recent error code. getReadErrorMessage() gets the most recent read error.
        //              getWriteErrorMessage() gets the most recent write error.
        // PARAMETERS:  Sinc *sc - the sinc connection.
        //

        std::string getErrorMessage() const        { return sinc_.err->msg; }
        std::string getReadErrorMessage() const    { return sinc_.readErr.msg; }
        std::string getWriteErrorMessage() const   { return sinc_.writeErr.msg; }


        //
        // NAME:        nextPacketType
        // ACTION:      Finds the packet type of the next packet.
        // PARAMETERS:  MessageType &msgType - the found packet type is placed here.
        // RETURNS:     true on success, false otherwise. On failure use currentErrorCode() and
        //                  currentErrorMessage() to get the error status.
        //

        bool nextPacketType(MessageType &msgType)
        {
            return SincPacketPeek(&sinc_, sinc_.timeout, &msgType);
        }



        //
        // NAME:        receive
        // ACTION:      Gets the next packet. May block if timeout is not specified.
        // PARAMETERS:  Buffer &buf - where to put the packet.
        //              MessageType &msgType - the found packet type is placed here.
        //              int timeout - timeout in ms. -1 for forever. 0 to poll.
        //                  If not specified will use the default sinc timeout.
        // RETURNS:     true on success, false otherwise. On failure use currentErrorCode() and
        //                  currentErrorMessage() to get the error status.
        //

        bool receive(Buffer &buf, MessageType &msgType, int timeout = -2)
        {
            return SincReadMessage(&sinc_, (timeout == -2) ? sinc_.timeout : timeout, &buf.getSincBuffer(), &msgType);
        }



        //
        // NAME:        send
        // ACTION:      send a buffer to this device. Would normally be encoded with requestXXX() beforehand.
        // PARAMETERS:  const Buffer &buf - the buffer to send.
        //

        bool send(Buffer &buf)
        {
            return SincSendNoFree(&sinc_, &buf.getSincBuffer());
        }


        //
        // Command "send request" functions which can be used stand-alone.
        // After calling these you must get the appropriate response using
        // waitXXX() or the protocol will be violated. Use doXXX() instead
        // if unsure.
        //
        // Before calling any of these you must call getCommandMutex()
        // and lock the mutex until after the response has been received
        // so multiple threads don't attempt to access the protocol at
        // the same time, which will cause a protocol violation and
        // likely loss of happiness.
        //

        bool requestPing(int showOnConsole)                           { Buffer buf; buf.encodePing(showOnConsole); return send(buf); }
        bool requestGetParam(int channelId, const std::string &name)  { Buffer buf; buf.encodeGetParam(channelId, name); return send(buf); }
        bool requestGetParams(std::vector<std::pair<int, std::string>> chanKeys) { Buffer buf; buf.encodeGetParams(chanKeys); return send(buf);}
        bool requestSetParam(int channelId, const KeyValue &param)    { Buffer buf; buf.encodeSetParam(channelId, param); return send(buf); }
        bool requestSetParams(int channelId, const std::vector<KeyValue> &params) { Buffer buf; buf.encodeSetParams(channelId, params); return send(buf); }
        bool requestStartCalibration(int channelId)                   { Buffer buf; buf.encodeStartCalibration(channelId); return send(buf); }
        bool requestGetCalibration(int channelId)                     { Buffer buf; buf.encodeGetCalibration(channelId); return send(buf); }
        bool requestSetCalibration(int channelId, const CalibrationInfo &calInfo) { Buffer buf; buf.encodeSetCalibration(channelId, calInfo); return send(buf); }
        bool requestCalculateDcOffset(int channelId)                  { Buffer buf; buf.encodeCalculateDcOffset(channelId); return send(buf); }
        bool requestStartOscilloscope(int channelId)                  { Buffer buf; buf.encodeStartOscilloscope(channelId); return send(buf); }
        bool requestStartHistogram(int channelId)                     { Buffer buf; buf.encodeStartHistogram(channelId); return send(buf); }
        bool requestClearHistogramData(int channelId)                 { Buffer buf; buf.encodeClearHistogramData(channelId); return send(buf); }
        bool requestStartListMode(int channelId)                      { Buffer buf; buf.encodeStartListMode(channelId); return send(buf); }
        bool requestStop(int channelId, bool skip = false)            { Buffer buf; buf.encodeStop(channelId, skip); return send(buf); }
        bool requestListParamDetails(int channelId)                   { Buffer buf; buf.encodeListParamDetails(channelId); return send(buf); }
        bool requestRestart()                                         { Buffer buf; buf.encodeRestart(); return send(buf); }
        bool requestResetSpatialSystem()                              { Buffer buf; buf.encodeResetSpatialSystem(); return send(buf); }
        bool requestSoftwareUpdate(const std::string &appImage, const std::string &appChecksum, const std::string &fpgaImage, const std::string &fpgaChecksum, bool autoRestart)
        {
            Buffer buf;
            buf.encodeSoftwareUpdate(appImage, appChecksum, fpgaImage, fpgaChecksum, autoRestart);

            return send(buf);
        }
        bool requestSaveConfiguration()                               { Buffer buf; buf.encodeSaveConfiguration(); return send(buf); }
        bool requestDeleteSavedConfiguration()                        { Buffer buf; buf.encodeDeleteSavedConfiguration(); return send(buf); }
        bool requestMonitorChannels(const std::vector<int> &channels) { Buffer buf; buf.encodeMonitorChannels(channels); return send(buf); }
        bool requestProbeDatagram()                                   { Buffer buf; buf.encodeProbeDatagram(); return send(buf); }
        bool requestDownloadCrashDump()                               { Buffer buf; buf.encodeDownloadCrashDump(); return send(buf); }


        //
        // waitXXX() calls wait for a particular packet type to be received.
        //

        bool waitReady(int channelId) { return SincWaitReady(&sinc_, channelId, sinc_.timeout); }

        bool waitResponse(SiToro__Sinc__MessageType responseType, Buffer &response) { return SincWaitForMessageType(&sinc_, sinc_.timeout, &response.getSincBuffer(), responseType); }

        bool waitSuccess()
        {
            Buffer buf;
            if (!waitResponse(SI_TORO__SINC__MESSAGE_TYPE__SUCCESS_RESPONSE, buf))
                return false;

            SuccessResponse resp;
            if (!buf.decodeSuccessResponse(sinc_.readErr, resp))
            {
                sinc_.err = &sinc_.readErr;
                return false;
            }

            return SincInterpretSuccess(&sinc_, resp.getSincSuccessResponse());
        }

        bool waitGetParamResponse(GetParamResponse &resp)
        {
            Buffer buf;
            if (!waitResponse(SI_TORO__SINC__MESSAGE_TYPE__GET_PARAM_RESPONSE, buf))
            {
                return false;
            }

            if (!buf.decodeGetParamResponse(sinc_.readErr, resp))
            {
                sinc_.err = &sinc_.readErr;
                return false;
            }

            return true;
        }

        bool waitGetCalibrationResponse(CalibrationInfo &calibInfo)
        {
            Buffer buf;
            if (!waitResponse(SI_TORO__SINC__MESSAGE_TYPE__GET_CALIBRATION_RESPONSE, buf))
                return false;

            if (!buf.decodeGetCalibrationResponse(sinc_.readErr, calibInfo))
            {
                sinc_.err = &sinc_.readErr;
                return false;
            }

            return true;
        }

        bool waitCalculateDcOffsetResponse(int &fromChannelId, double &dcOffset)
        {
            Buffer buf;
            if (!waitResponse(SI_TORO__SINC__MESSAGE_TYPE__CALCULATE_DC_OFFSET_RESPONSE, buf))
                return false;

            if (!buf.decodeCalculateDcOffsetResponse(sinc_.readErr, fromChannelId, dcOffset))
            {
                sinc_.err = &sinc_.readErr;
                return false;
            }

            return true;
        }

        bool waitCalibrationComplete(int channelId)
        {
            Buffer buf;
            int complete = false;
            while (!complete)
            {

                if (!waitResponse(SI_TORO__SINC__MESSAGE_TYPE__CALIBRATION_PROGRESS_RESPONSE, buf))
                    return false;

                CalibrationProgressResponse resp;
                if (!buf.decodeCalibrationProgressResponse(sinc_.readErr, resp))
                {
                    sinc_.err = &sinc_.readErr;
                    return false;
                }

                complete = resp.getComplete();
            }

            if (!waitReady(channelId))
                return false;

            return true;
        }

        bool waitListParamDetailsResponse(ParamDetails &details)
        {
            Buffer buf;
            if (!waitResponse(SI_TORO__SINC__MESSAGE_TYPE__LIST_PARAM_DETAILS_RESPONSE, buf))
                return false;

            if (!buf.decodeListParamDetailsResponse(sinc_.readErr, details))
            {
                sinc_.err = &sinc_.readErr;
                return false;
            }

            return true;
        }


        bool waitDownloadCrashDumpResponse(DownloadCrashDump &resp)
        {
            Buffer buf;
            if (!waitResponse(SI_TORO__SINC__MESSAGE_TYPE__DOWNLOAD_CRASH_DUMP_RESPONSE, buf))
                return false;

            if (!buf.decodeDownloadCrashDumpResponse(sinc_.readErr, resp))
            {
                sinc_.err = &sinc_.readErr;
                return false;
            }
            return true;
        }


        //
        // NAME:        waitFdSet
        // ACTION:      Wait until data is available for reading from the device on one
        //              of a number of sockets.
        // PARAMETERS:  std::vector<bool> &ready - will be sized like fdSet and set to true if data was received
        //                  on the corresponding fd.
        //              bool &timedOut - will be set to true if it timed out waiting on input.
        //              const std::vector<int> &fdSet - the fds to be waited on. Get using Sinc::waitFd().
        //              int timeout - in milliseconds. 0 to poll. -1 to wait forever.
        //                  If not specified will use the default sinc timeout.
        //              bool *readOk - an array corresponding to the fd array. Set to true if data is available to read.
        // RETURNS:     true on success or timeout, false on error. Error will be set.
        //

        bool waitFdSet(std::vector<bool> &ready, bool &timedOut, const std::vector<int> &fdSet, int timeout = -2)
        {
            ready.resize(fdSet.size());
            timedOut = false;

            bool readies[fdSet.size()];
            int result = SincSocketWaitMulti(fdSet.data(), fdSet.size(), (timeout == -2) ? sinc_.timeout : timeout, readies);
            if (result == SI_TORO__SINC__ERROR_CODE__TIMEOUT)
            {
                // Timed out.
                timedOut = true;
                return true;
            }
            else if (result != 0)
            {
                // Errored.
                SincErrorSetCode(&sinc_.readErr, static_cast<SiToro__Sinc__ErrorCode>(result));
                sinc_.err = &sinc_.readErr;
                return false;
            }

            // Copy the booleans.
            for (size_t i = 0; i < fdSet.size(); i++)
            {
                ready[i] = readies[i];
            }

            return true;
        }

    };
}

#endif /* SINCPLUSPLUS_H */
