#pragma once

#include "deformer_base.hpp"
#include "deformable.hpp"
#include "curve.hpp"
#include "deformer/drivers/phys.hpp"
#include "../param/parameter.hpp"

#include <memory>
#include <vector>
#include <unordered_map>

namespace nicxlive::core::nodes {

class PathDeformer : public Deformable, public Deformer {
public:
    enum class CurveType { Bezier, Spline };
    enum class PhysicsType { Pendulum, SpringPendulum };

    class PhysicsDriver {
    public:
        virtual ~PhysicsDriver() = default;
        virtual void setup() {}
        virtual void update(Vec2Array& offsets, float strength, uint64_t frame) = 0;
        virtual void updateDefaultShape() {}
        virtual void reset() {}
        virtual void enforce(const Vec2&) {}
        virtual void rotate(float) {}
        virtual void getState(PhysicsDriverState&) const {}
    };
    class ConnectedDriverAdapter : public PhysicsDriver {
    public:
        explicit ConnectedDriverAdapter(std::unique_ptr<ConnectedPhysicsDriver> impl, PathDeformer* owner)
            : impl_(std::move(impl)), owner_(owner) {}
        void setup() override;
        void update(Vec2Array& offsets, float strength, uint64_t frame) override;
        void updateDefaultShape() override;
        void reset() override;
        void enforce(const Vec2& f) override;
        void rotate(float a) override;
        void getState(PhysicsDriverState& out) const override { if (impl_) impl_->getState(out); }
        void setState(const PhysicsDriverState& st) { if (impl_) impl_->setState(st); }
        ConnectedPhysicsDriver* impl() const { return impl_.get(); }
    private:
        std::unique_ptr<ConnectedPhysicsDriver> impl_{};
        PathDeformer* owner_{nullptr};
    };
    class PendulumDriver : public PhysicsDriver {
    public:
        void update(Vec2Array& offsets, float strength, uint64_t frame) override;
    };
    class SpringPendulumDriver : public PhysicsDriver {
    public:
        float velocity{0.0f};
        void update(Vec2Array& offsets, float strength, uint64_t frame) override;
    };

    std::vector<Vec2> controlPoints{};
    std::vector<Vec2> curveSamples{};
    std::vector<Vec2> curveNormals{};
    std::vector<float> curveParams{};
    Mat4 inverseMatrix{Mat4::identity()};
    float strength{0.0f};
    bool translateChildren{true};
    bool dynamicDeformation{false};
    float maxSegmentLength{0.0f};
    float totalLength{0.0f};
    CurveType curveType{CurveType::Spline};
    PhysicsType physicsType{PhysicsType::Pendulum};
    uint64_t frameCounter{0};
    std::unique_ptr<PhysicsDriver> driver{};
    std::unique_ptr<Curve> originalCurve{};
    std::unique_ptr<Curve> deformedCurve{};
    std::unordered_map<const Node*, std::vector<float>> meshCaches{};
    std::unique_ptr<Curve> prevCurve{};
    Vec2 prevRoot{};
    bool prevRootSet{false};
    bool driverInitialized{false};
    bool physicsOnly{false};
    bool matrixInvalidThisFrame{false};
    uint64_t consecutiveInvalidFrames{0};
    bool diagnosticsFrameActive{false};
    std::vector<uint64_t> invalidLastLoggedFrame{};
    std::vector<uint64_t> invalidLastLoggedCount{};
    std::vector<bool> invalidLastLoggedValueWasNaN{};
    std::vector<Vec2> invalidLastLoggedValue{};
    std::vector<std::string> invalidLastLoggedContext{};
    bool hasDegenerateBaseline{false};
    std::vector<std::size_t> degenerateSegmentIndices{};
    std::vector<bool> invalidIndexThisFrame{};
    std::vector<std::size_t> invalidStreakStartFrame{};
    std::vector<std::size_t> invalidLastLoggedStreak{};
    std::vector<float> curveDiagTargetScale{};
    std::vector<float> curveDiagReferenceScale{};
    std::vector<bool> curveDiagHasNaN{};
    std::vector<bool> curveDiagCollapsed{};
    struct DiagnosticsState {
        uint64_t invalidFrameCount{0};
        uint64_t totalInvalidCount{0};
        bool invalidThisFrame{false};
        bool diagnosticsFrameActive{false};
        std::vector<uint64_t> invalidTotalPerIndex{};
        std::vector<uint64_t> invalidConsecutivePerIndex{};
        std::vector<bool> invalidIndexThisFrame{};
        std::vector<std::size_t> invalidStreakStartFrame{};
        std::vector<uint64_t> invalidLastLoggedFrame{};
        std::vector<uint64_t> invalidLastLoggedCount{};
        std::vector<bool> invalidLastLoggedValueWasNaN{};
        std::vector<Vec2> invalidLastLoggedValue{};
        std::vector<std::string> invalidLastLoggedContext{};
    } diagnostics{};
    // diagnostics (簡易)
    uint64_t invalidFrameCount{0};
    uint64_t totalInvalidCount{0};
    bool invalidThisFrame{false};
    std::vector<uint64_t> invalidPerIndex{};
    std::vector<uint64_t> invalidConsecutive{};
    std::vector<uint64_t> invalidLastFrame{};
    std::vector<Vec2> invalidLastValue{};
    bool diagnosticsEnabled{false};
    float lastMaxOffset{0.0f};
    float lastAvgOffset{0.0f};
    std::string lastInvalidContext{};
    struct InvalidRecord {
        uint64_t frame{0};
        std::string context{};
        Vec2 value{};
    };
    std::vector<InvalidRecord> invalidLog{};
    bool preserveInvalidLog{false};

    PathDeformer();

    const std::string& typeId() const override {
        static const std::string k = "PathDeformer";
        return k;
    }

    DeformResult deformChildren(const std::shared_ptr<Node>& target,
                                const std::vector<Vec2>& origVertices,
                                const std::vector<Vec2>& origDeformation,
                                const Mat4* origTransform) override;

    // Node overrides
    void runPreProcessTask(core::RenderContext& ctx) override;
    void runRenderTask(core::RenderContext& ctx) override;
    bool setupChild(const std::shared_ptr<Node>& child) override;
    bool releaseChild(const std::shared_ptr<Node>& child) override;
    void captureTarget(const std::shared_ptr<Node>& target) override;
    void releaseTarget(const std::shared_ptr<Node>& target) override;
    void clearCache() override;
    void build(bool force = false) override;
    bool mustPropagate() const override { return true; }
    bool coverOthers() const override { return true; }
    void notifyChange(const std::shared_ptr<Node>& target, NotifyReason reason = NotifyReason::AttributeChanged) override;
    void applyDeformToChildren(const std::vector<std::shared_ptr<core::param::Parameter>>& params, bool recursive = true) override;
    void copyFrom(const Node& src, bool clone = false, bool deepCopy = true) override;
    void rebuffer(const std::vector<Vec2>& points);
    std::unique_ptr<PhysicsDriver> createPhysicsDriver();
    bool physicsEnabled() const { return static_cast<bool>(driver); }
    void setStrength(float s) { strength = s; }
    void setPhysicsEnabled(bool value) {
        if (value) {
            if (!driver) {
                driver = createPhysicsDriver();
            }
        } else {
            driver.reset();
        }
        driverInitialized = false;
        prevRootSet = false;
    }
    void setTranslateChildren(bool v) { translateChildren = v; }
    void setCurveType(CurveType t) { curveType = t; clearCache(); }
    void setPhysicsType(PhysicsType t) { physicsType = t; }
    void setPhysicsOnly(bool v) { physicsOnly = v; }
    void setDynamicDeformation(bool v) { dynamicDeformation = v; }
    void reportInvalid(const std::string& ctx, std::size_t idx, const Vec2& value);
    // D互換の物理デグレ報告
    void reportPhysicsDegeneracy(const std::string& ctx) { disablePhysicsDriver(ctx); }
    void switchDynamic(bool enablePhysics);
    void serializeSelfImpl(::nicxlive::core::serde::InochiSerializer& serializer, bool recursive, SerializeNodeFlags flags) const override;
    ::nicxlive::core::serde::SerdeException deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data) override;

private:
    bool setupChildNoRecurse(const std::shared_ptr<Node>& node, bool prepend = false);
    bool releaseChildNoRecurse(const std::shared_ptr<Node>& node);
    void rebuildCurveSamples();
    void ensureDriver();
    Vec2 projectToCurve(const Vec2& p) const;
    Vec2 evaluateCurve(float t) const;
    Vec2 evaluateTangent(float t) const;
    float curveLength() const;
    void recordInvalid(const std::string& ctx, std::size_t idx, const Vec2& value);
    void logDiagnostics() const;
    void sanitizeOffsets(Vec2Array& offsets);
    void validateCurve();
    void resetDiagnostics();
    void cacheClosestPoints(const std::shared_ptr<Node>& node, int nSamples = 100);
    void disablePhysicsDriver(const std::string& reason);
    Vec2 sanitizeVec2(const Vec2& v) const;
    void applyPathDeform(const Vec2Array& origDeform);
    void deform(const Vec2Array& deformedControlPoints);
    void logCurveState(const std::string& ctx);
    void logCurveHealth(const std::string& ctx, const std::unique_ptr<Curve>& a, const std::unique_ptr<Curve>& b, const Vec2Array& def);
    bool shouldEmitInvalidIndexLog(std::size_t index, const std::string& context, const Vec2& value, std::size_t consecutive);
    void logInvalidIndex(const std::string& context, std::size_t index, const Vec2& value, std::size_t consecutive);
    bool matrixIsFinite(const Mat4& m) const { return isFiniteMatrix(m); }
    bool beginDiagnosticFrame();
    void endDiagnosticFrame();
    void logTransformFailure(const std::string& ctx, const Mat4& m);
    void refreshInverseMatrix(const std::string& ctx);
    void markInvalidOffset(const std::string& ctx, std::size_t idx, const Vec2& value);
    void logInvalidSnapshot(const std::string& ctx, const Vec2Array& deform);
    void logCurveDiag(const std::string& ctx, const Vec2Array& orig, const Vec2Array& deform);
    void ensureDiagnosticCapacity(std::size_t n);
    void checkBaselineDegeneracy(const std::vector<Vec2>& pts);
    void clearInvalidFlags();
    void beginInvalidFrame();
    void endInvalidFrame();
};

} // namespace nicxlive::core::nodes
