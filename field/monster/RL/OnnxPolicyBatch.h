#pragma once
#include <onnxruntime_cxx_api.h>
#include <onnxruntime_c_api.h>
#include <vector>
#include <string>
#include <cassert>
static std::string GetInputName_C(Ort::Session& session, int idx)
{
    Ort::AllocatorWithDefaultOptions alloc;
    Ort::AllocatedStringPtr name = session.GetInputNameAllocated(idx, alloc);
    return std::string(name.get());
}

static std::string GetOutputName_C(Ort::Session& session, int idx)
{
    Ort::AllocatorWithDefaultOptions alloc;
    Ort::AllocatedStringPtr name = session.GetOutputNameAllocated(idx, alloc);
    return std::string(name.get());
}


class OnnxPolicyBatch {
public:

    OnnxPolicyBatch(const std::wstring& modelPath, int obsDim = 16, int actDim = 5, int intraThreads = 1)
        : obsDim_(obsDim)
        , actDim_(actDim)
        , env_(ORT_LOGGING_LEVEL_WARNING, "rl")
        , mem_(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault))
    {
        Ort::SessionOptions so;
        so.SetIntraOpNumThreads(intraThreads);
        so.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        session_ = Ort::Session(env_, modelPath.c_str(), so);

        // 이름은 char* 받아서 std::string으로 복사 후 Free
        inName_ = GetInputName_C(session_, 0);                

        outLogitsName_ = GetOutputName_C(session_, 0);
                
        outValueName_ = GetOutputName_C(session_, 1);
    }

    void RunBatch(int N, const float* obsBatch, std::vector<float>& logitsBatch, std::vector<float>* valueBatch = nullptr)
    {
        assert(N > 0);
        logitsBatch.resize((size_t)N * (size_t)actDim_);
        if (valueBatch) valueBatch->resize((size_t)N);

        std::vector<int64_t> inShape{ (int64_t)N, (int64_t)obsDim_ };
        Ort::Value input = Ort::Value::CreateTensor<float>(
            mem_,
            const_cast<float*>(obsBatch),
            (size_t)N * (size_t)obsDim_,
            inShape.data(),
            inShape.size()
        );

        std::vector<int64_t> logitsShape{ (int64_t)N, (int64_t)actDim_ };
        Ort::Value outLogits = Ort::Value::CreateTensor<float>(
            mem_,
            logitsBatch.data(),
            logitsBatch.size(),
            logitsShape.data(),
            logitsShape.size()
        );

        // value는 안 쓰더라도 모델 output이 2개라 같이 받는 게 안전
        scratchValue_.resize((size_t)N);
        std::vector<int64_t> valueShape{ (int64_t)N, 1 };
        Ort::Value outValue = Ort::Value::CreateTensor<float>(
            mem_,
            scratchValue_.data(),
            scratchValue_.size(),
            valueShape.data(),
            valueShape.size()
        );

        const char* inputNames[] = { inName_.c_str() };
        const char* outputNames[] = { outLogitsName_.c_str(), outValueName_.c_str() };
        Ort::Value outputs[] = { std::move(outLogits), std::move(outValue) };

        session_.Run(
            Ort::RunOptions{ nullptr },
            inputNames, &input, 1,
            outputNames, outputs, 2
        );

        if (valueBatch) {
            *valueBatch = scratchValue_; // 필요할 때만 복사
        }
    }

private:
    int obsDim_;
    int actDim_;

    Ort::Env env_;
    Ort::Session session_{ nullptr };
    Ort::MemoryInfo mem_;
    Ort::AllocatorWithDefaultOptions alloc_;

    std::string inName_;
    std::string outLogitsName_;
    std::string outValueName_;

    std::vector<float> scratchValue_;
};
