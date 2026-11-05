#pragma once

#include "deformer_base.hpp"
#include "deformable.hpp"
#include "curve.hpp"
#include "../compat/native_access_fault.hpp"
#include "../common/object_key_policy.hpp"
#include "../common/resource_key_map.hpp"
#include "../compat/phobos_string_hash.hpp"
#include "deformer/drivers/phys.hpp"
#include "../param/parameter.hpp"
#include "../compat/source_allocator.hpp"
#include <functional>
#include <utility>

#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>
#include <unordered_map>

namespace nicxlive::core::nodes {

class PathDeformer : public Deformable, public Deformer, public SourceDeformation {
public:
    enum class CurveType { Bezier, Spline };
    enum class PhysicsType { Pendulum, SpringPendulum };

    using Deformable::getVertices;
    Vec2Array& getVertices() override {
        return d_runtime::ngRequireObject(originalCurve)->controlPoints();
    }

    virtual std::shared_ptr<Curve> createCurve(Vec2Array points) {
        if (curveType == CurveType::Bezier) return ngCreateBezierCurveSource(points);
        return ngCreateSplineCurveSource(points);
    }

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
        virtual void retarget(std::shared_ptr<PathDeformer> deformer) = 0;
        virtual void serializeSource(core::serde::TypedJsonWriter&) const {
            throw std::logic_error("Native-only physics driver has no Source serialization");
        }
        virtual core::serde::ReadError deserializeSource(core::serde::TypedJsonView) {
            throw std::logic_error("Native-only physics driver has no Source deserialization");
        }
        virtual void copyFromSource(std::shared_ptr<PhysicsDriver>) {
            throw std::logic_error("Native-only physics driver has no Source copyFrom");
        }
    };
    class ConnectedDriverAdapter : public PhysicsDriver {
    public:
        explicit ConnectedDriverAdapter(std::unique_ptr<ConnectedPhysicsDriver> impl, PathDeformer* owner)
            : impl_(std::move(impl)) { (void)owner; }
        void setup() override;
        void update(Vec2Array& offsets, float strength, uint64_t frame) override;
        void updateDefaultShape() override;
        void reset() override;
        void retarget(std::shared_ptr<PathDeformer> deformer) override;
        void enforce(const Vec2& f) override;
        void rotate(float a) override;
        void getState(PhysicsDriverState& out) const override { if (impl_) impl_->getState(out); }
        void setState(const PhysicsDriverState& st) { if (impl_) impl_->setState(st); }
        ConnectedPhysicsDriver* impl() const { return impl_.get(); }
        void serializeSource(core::serde::TypedJsonWriter& writer) const override;
        core::serde::ReadError deserializeSource(core::serde::TypedJsonView data) override;
        void copyFromSource(std::shared_ptr<PhysicsDriver> source) override;
    private:
        std::unique_ptr<ConnectedPhysicsDriver> impl_{};
    };
    class PendulumDriver : public PhysicsDriver {
    public:
        void retarget(std::shared_ptr<PathDeformer>) override { setup(); }
        void update(Vec2Array& offsets, float strength, uint64_t frame) override;
    };
    class SpringPendulumDriver : public PhysicsDriver {
    public:
        void retarget(std::shared_ptr<PathDeformer>) override { setup(); }
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
    std::size_t frameCounter{0};
    std::shared_ptr<PhysicsDriver> driver{};
    std::shared_ptr<Curve> originalCurve{};
    std::shared_ptr<Curve> deformedCurve{};
    using MeshCache = common::PolicyKeyMap<std::shared_ptr<Node>, common::SharedArray<float>,
        common::ObjectClassKeyPolicy<std::shared_ptr<Node>>>;
    MeshCache meshCaches{};
    std::shared_ptr<Curve> prevCurve{};
#ifdef NIJIGENERATE_DEBUG_PATH_DEFORM
    // D AA descriptors retain both the table and the Node keys.
    using ClosestPointsMap = common::PolicyKeyMap<std::shared_ptr<Node>, Vec2Array,
        common::ObjectClassKeyPolicy<std::shared_ptr<Node>>>;
    using ClosestPoints = std::shared_ptr<ClosestPointsMap>;
    ClosestPoints closestPointsDeformed;
    ClosestPoints closestPointsOriginal;
#endif
    Vec2 prevRoot{};
    bool prevRootSet{false};
    bool driverInitialized{false};
    bool physicsOnly{false};
    bool matrixInvalidThisFrame{false};
    std::size_t consecutiveInvalidFrames{0};
    bool diagnosticsFrameActive{false};
    common::SharedArray<std::size_t> invalidLastLoggedFrame{};
    common::SharedArray<std::size_t> invalidLastLoggedCount{};
    common::SharedArray<bool> invalidLastLoggedValueWasNaN{};
    Vec2Array invalidLastLoggedValue{};
    common::SharedArray<common::NullableString> invalidLastLoggedContext{};
    bool hasDegenerateBaseline{false};
    common::SharedArray<std::size_t> degenerateSegmentIndices{};
    common::SharedArray<bool> invalidIndexThisFrame{};
    common::SharedArray<std::size_t> invalidStreakStartFrame{};
    std::vector<std::size_t> invalidLastLoggedStreak{};
    std::vector<float> curveDiagTargetScale{};
    std::vector<float> curveDiagReferenceScale{};
    std::vector<bool> curveDiagHasNaN{};
    std::vector<bool> curveDiagCollapsed{};
    Vec2Array sampleScratch_{};
    Vec2Array closestOrigScratch_{};
    Vec2Array closestDefScratch_{};
    Vec2Array tangentOrigScratch_{};
    Vec2Array tangentDefScratch_{};
    std::vector<float> tSamplesScratch_{};
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
    std::size_t invalidFrameCount{0};
    uint64_t totalInvalidCount{0};
    bool invalidThisFrame{false};
    std::size_t totalMatrixInvalidCount{0};
    bool loggedTransformInvalid{false};
    common::SharedArray<std::size_t> invalidPerIndex{};
    common::SharedArray<std::size_t> invalidConsecutive{};
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

    explicit PathDeformer(CurveType type = CurveType::Spline);
    static std::shared_ptr<PathDeformer> create(std::shared_ptr<Node> parent = nullptr,
        CurveType type = CurveType::Spline);
    template<class ParentProducer>
    static std::shared_ptr<PathDeformer> createSourceDeferred(ParentProducer&& produceParent) {
        auto receiver = allocateSourcePathReceiver();
        std::shared_ptr<Node> parent = std::invoke(std::forward<ParentProducer>(produceParent));
        receiver->constructPathOwned(std::move(parent), CurveType::Spline);
        return receiver;
    }

    common::NullableString typeId() const override {
        return common::NullableString::fromStaticLiteral("PathDeformer");
    }

    DeformResult deformChildren(const std::shared_ptr<Node>& target,
                                const Vec2Array& origVertices,
                                Vec2Array& origDeformation,
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
    void notifyChange(const std::shared_ptr<Node>& target, NotifyReason reason = NotifyReason::Transformed) override;
    void runDynamicTask(core::RenderContext& context) override;
    void centralize() override;
    void dispose() override;
    void applyDeformToChildren(common::SharedArray<std::shared_ptr<core::param::Parameter>> params, bool recursive = true) override;
    void copyFrom(const Node& src, bool clone = false, bool deepCopy = true) override;
    void rebuffer(const std::vector<Vec2>& points);
    void rebuffer(const Vec2Array& points) override;
    std::unique_ptr<PhysicsDriver> createPhysicsDriver();
    void setStrength(float s) { strength = s; }
    void setTranslateChildren(bool v) { translateChildren = v; }
    void setCurveType(CurveType t) { curveType = t; clearCache(); }
    void setPhysicsType(PhysicsType t) { physicsType = t; }
    void setPhysicsOnly(bool v) { physicsOnly = v; }
    void setDynamicDeformation(bool v) { dynamicDeformation = v; }
    void reportInvalid(const std::string& ctx, std::size_t idx, const Vec2& value);
    // D互換の物理デグレ報告
    void serializeSelfImpl(::nicxlive::core::serde::InochiSerializer& serializer, bool recursive, SerializeNodeFlags flags) const override;
    ::nicxlive::core::serde::SerdeException deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data) override;
    void serializeSelfImpl(::nicxlive::core::serde::TypedJsonWriter& writer, bool recursive,
        SerializeNodeFlags flags) const override;
    ::nicxlive::core::serde::ReadError deserializeFromFghj(
        ::nicxlive::core::serde::TypedJsonView data) override;


    // Source path.d1254 is non-final; append without moving any old virtual slot.
    virtual bool physicsEnabled() const { return static_cast<bool>(driver); }

    SourceDeformResult deformChildrenSource(std::shared_ptr<Node> target,
        Vec2Array originalVertices, Vec2Array originalDeformation, Mat4* originalTransform) override;
    virtual std::shared_ptr<PhysicsDriver> createPhysicsSource();
    virtual std::shared_ptr<PhysicsDriver> getDriverSource() { return driver; }
    virtual void setDriverSource(std::shared_ptr<PhysicsDriver> value);
    virtual void setPhysicsEnabled(bool value) {
        if (value) {
            if (!getDriverSource()) setDriverSource(createPhysicsSource());
        } else setDriverSource({});
    }
    virtual void switchDynamic(bool dynamic);
    virtual void reportPhysicsDegeneracy(const std::string& context) { sourceDisablePhysicsDriver(context); }
    virtual void reportDriverInvalidSource(std::string context, std::size_t index, Vec2 value);
    // Source setupChildNoRecurse is a public, nonvirtual template.
    bool setupChildNoRecurse(std::shared_ptr<Node> node, bool prepend = false);

protected:
    struct CurveDiagLogState {
        bool hasNaN{false};
        bool collapsed{false};
        float targetScale{std::numeric_limits<float>::quiet_NaN()};
        float referenceScale{std::numeric_limits<float>::quiet_NaN()};
        std::size_t lastFrame{0};
        std::size_t firstInvalidFrame{0};
    };
    struct CurveContextHash {
        std::size_t operator()(const common::NullableString& value) const noexcept {
            return d_runtime::ngPhobosStringHash(value.view());
        }
    };
    struct CurveContextEqual {
        bool operator()(const common::NullableString& left, const common::NullableString& right) const noexcept {
            return left == right;
        }
    };
    common::ResourceKeyMap<common::NullableString, CurveDiagLogState,
        CurveContextHash, CurveContextEqual> curveDiagLogStates;
    virtual void deformSource(Vec2Array points);
    virtual Vec2Array getVerticesSource(std::shared_ptr<Node> node);
    virtual void cacheClosestPointsSource(std::shared_ptr<Node> node, int samples = 100);
    template<class> friend struct d_runtime::SourceAllocator;
    explicit PathDeformer(DeferredConstruction construction)
        : Deformable(construction), curveType(CurveType::Bezier) {
        const auto nan = std::numeric_limits<float>::quiet_NaN();
        prevRoot = {nan, nan};
        for (auto& row : inverseMatrix.a.a)
            for (auto& value : row) value = nan;
    }
    void constructPathOwned(std::shared_ptr<Node> parent, CurveType type);

private:
    static std::shared_ptr<PathDeformer> allocateSourcePathReceiver();
    void sourceSanitizeOffsets(Vec2Array& values);
    void sourceRefreshInverseMatrix(const std::string& context);
    void sourceLogTransformFailure(const std::string& context, const Transform& transform);
    bool sourceLogCurveHealth(std::string context, std::shared_ptr<Curve> reference,
        std::shared_ptr<Curve> target, const Vec2Array& snapshot);
    void sourceLogCurveState(const std::string& context);
    void sourceCheckBaselineDegeneracy(const Vec2Array& points);
    void sourceHandleInvalidDeformation(const std::string& context, const Vec2Array fallback);
    void sourceApplyPathDeform(const Vec2Array original);
    void sourceEnsureDiagnosticCapacity(std::size_t length);
    bool sourceBeginDiagnosticFrame();
    void sourceEndDiagnosticFrame();
    void sourceMarkInvalidOffset(const std::string& context, std::size_t index, Vec2 value);
    void sourceDisablePhysicsDriver(const std::string& context);
    Mat4 sourceRequireFiniteMatrix(Mat4 matrix, const std::string& context);
    Mat4 sourceSafeInverse(Mat4 matrix, const std::string& context);
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
    void logCurveHealth(const std::string& ctx, const std::shared_ptr<Curve>& a, const std::shared_ptr<Curve>& b, const Vec2Array& def);
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

public:
    // Append the original public virtual release after every existing virtual declaration.
    virtual bool releaseChildNoRecurse(std::shared_ptr<Node> node);
};

} // namespace nicxlive::core::nodes
