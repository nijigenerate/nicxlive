#include "simple_physics_driver.hpp"
#include "../phys/system.hpp"

namespace nicxlive::core::nodes {
void SimplePhysicsDriver::copyFrom(const Node& src, bool clone, bool deepCopy) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    [[maybe_unused]] const auto sourceLifetime = src.weak_from_this().lock();
    Driver::copyFrom(src, clone, deepCopy);
    if (const auto* physics = dynamic_cast<const SimplePhysicsDriver*>(&src)) {
        modelType = physics->modelType;
        mapMode = physics->mapMode;
        localOnly = physics->localOnly;
        gravity = physics->gravity;
        length = physics->length;
        frequency = physics->frequency;
        angleDamping = physics->angleDamping;
        lengthDamping = physics->lengthDamping;
        outputScale = physics->outputScale;
        prevAnchorSet = false;
        anchor = Vec2{0, 0};
        reset();
        paramRef = physics->paramRef;
        param_ = physics->param_;
        paramCached = param_;
    }
}
}
#include "../serde.hpp"
#include "../param/parameter.hpp"
#include "../puppet.hpp"
#include "../nodes/common.hpp"
#include "../timing.hpp"
#include "../dbg.hpp"
#include "../debug_log.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <limits>
#include <vector>

namespace nicxlive::core::nodes {

namespace {
double deltaTime() {
    return ::nicxlive::core::deltaTime();
}

bool isFiniteVec(const Vec2& v) {
    return std::isfinite(v.x) && std::isfinite(v.y);
}

Vec4 transformPhysicsPoint(Mat4 matrix, Vec3 point) {
    // inmath Matrix * Vector clears the accumulator and folds columns in order.
    const std::array input{point.x, point.y, point.z, 1.0f};
    std::array<float, 4> result{};
    for (std::size_t column = 0; column < 4; ++column) {
        for (std::size_t row = 0; row < 4; ++row) {
            const auto product = math::detail::inmathProduct(matrix.a.a[row][column], input[column]);
            result[row] = math::detail::inmathAccumulate(result[row], product);
        }
    }
    return {result[0], result[1], result[2], result[3]};
}

template<class Value> void deserializePhysicsStringEnum(core::serde::TypedJsonView data, Value& value) {
    // FGHJ deserializeValue(enum) borrows a scoped string and ignores the
    // returned kind error before parsing. The retained view keeps these bytes alive.
    std::string_view text;
    const auto kind = data.kind();
    if (kind == core::serde::JsonKind::String) text = data.stringBytes();
    else if (kind != core::serde::JsonKind::Null) (void)core::serde::unexpectedJsonKind(data);
    for (const auto& member : Value::declaredMembers()) {
        if (text == member.name) { value = member.tag; return; }
    }
    d_runtime::RetainedString message;
    try {
        std::string bytes = "Unable to deserialize string '";
        bytes.append(text);
        bytes.append("' to ");
        bytes.append(Value::sourceTypeName());
        bytes.append("Allowed keys:allowedKeys");
        message = d_runtime::RetainedString(std::move(bytes));
    } catch (const std::bad_alloc&) {
        throw d_runtime::ngStaticError<d_runtime::OutOfMemoryError>(
            d_runtime::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
    } catch (const std::length_error&) {
        throw d_runtime::ngStaticError<d_runtime::OutOfMemoryError>(
            d_runtime::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
    }
    throw core::serde::JsonConversionError(std::move(message),
        d_runtime::RetainedString::fromStaticLiteral("fghj/serialization.d"), 2167);
}

bool parsePhysicsModel(const std::string& value, SourcePhysicsModel& out) {
    if (value == "pendulum" || value == "Pendulum") {
        out = PhysicsModel::Pendulum;
        return true;
    }
    if (value == "spring_pendulum" || value == "SpringPendulum") {
        out = PhysicsModel::SpringPendulum;
        return true;
    }
    return false;
}

bool parseParamMapMode(const std::string& value, SourceParamMapMode& out) {
    if (value == "angle_length" || value == "AngleLength") {
        out = ParamMapMode::AngleLength;
        return true;
    }
    if (value == "xy" || value == "XY") {
        out = ParamMapMode::XY;
        return true;
    }
    if (value == "length_angle" || value == "LengthAngle") {
        out = ParamMapMode::LengthAngle;
        return true;
    }
    if (value == "yx" || value == "YX") {
        out = ParamMapMode::YX;
        return true;
    }
    return false;
}

bool traceParamBindingEnabled() {
    static int enabled = -1;
    if (enabled >= 0) return enabled != 0;
    const char* v = std::getenv("NJCX_TRACE_PARAM_BIND");
    if (!v) {
        enabled = 0;
        return false;
    }
    enabled = (std::strcmp(v, "1") == 0 || std::strcmp(v, "true") == 0 || std::strcmp(v, "TRUE") == 0) ? 1 : 0;
    return enabled != 0;
}

} // namespace

// Native spelling adapter uses the one canonical Source RK4 implementation.
class PhysicsSystem : public ::nicxlive::phys::PhysicsSystem {
public:
    void drawDebug(Mat4 transform = Mat4::identity()) override { drawDebugNative(transform); }
    virtual void drawDebugNative(const Mat4& transform) = 0;
    void addVariableNative(float* value) { ::nicxlive::phys::PhysicsSystem::addVariable(value); }
    void addVariableNative(Vec2* value) { ::nicxlive::phys::PhysicsSystem::addVariable(value); }
    void setD(std::size_t index, float value) {
        ::nicxlive::phys::PhysicsSystem::setD(static_cast<std::uint64_t>(index), value);
    }
    void setD(std::size_t index, const Vec2& value) {
        setD(index, value.x);
        setD(index + 1, value.y);
    }
protected:
    using ::nicxlive::phys::PhysicsSystem::refs;
    using ::nicxlive::phys::PhysicsSystem::derivative;
};

class PendulumSystem : public PhysicsSystem {
public:
    explicit PendulumSystem(SimplePhysicsDriver* d) : driver(d) {
        if (driver) {
            const auto anchor = driver->anchor;
            const auto length = driver->getLength();
            bob = Vec2{anchor.x + 0.0f, anchor.y + length};
        } else {
            bob = Vec2{0, 0};
        }
        addVariableNative(&angle);
        addVariableNative(&dAngle);
    }

    void eval(float /*t*/) override {
        if (!driver) return;
        setD(0, dAngle);
        float gravityVal = driver->getGravity();
        float lengthVal = driver->getLength();
        if (!std::isfinite(gravityVal) || !std::isfinite(lengthVal) || std::fabs(lengthVal) <= std::numeric_limits<float>::epsilon()) {
            driver->logPhysicsState("Pendulum:invalidParams");
            setD(1, 0.0f);
            return;
        }

        float lengthRatio = gravityVal / lengthVal;
        if (!std::isfinite(lengthRatio) || lengthRatio < 0) {
            driver->logPhysicsState("Pendulum:lengthRatioInvalid");
            setD(1, 0.0f);
            return;
        }

        float critDamp = 2.0f * std::sqrt(lengthRatio);
        if (!std::isfinite(critDamp)) {
            driver->logPhysicsState("Pendulum:critDampInvalid");
            setD(1, 0.0f);
            return;
        }

        float angleDampingVal = driver->getAngleDamping();
        if (!std::isfinite(angleDampingVal)) {
            driver->logPhysicsState("Pendulum:angleDampingInvalid");
            setD(1, 0.0f);
            return;
        }

        float dd = -lengthRatio * std::sin(angle);
        if (!std::isfinite(dd)) {
            driver->logPhysicsState("Pendulum:ddInitialInvalid");
            dd = 0;
        }
        dd -= dAngle * angleDampingVal * critDamp;
        if (!std::isfinite(dd)) {
            driver->logPhysicsState("Pendulum:ddDampedInvalid");
            dd = 0;
        }
        setD(1, dd);
    }

    void tick(float h) {
        if (!driver) return;
        Vec2 dBob{bob.x - driver->anchor.x, bob.y - driver->anchor.y};
        if (!isFiniteVec(dBob)) {
            driver->logPhysicsState("Pendulum:dBobNonFinite");
            return;
        }
        angle = std::atan2(-dBob.x, dBob.y);
        if (!std::isfinite(angle)) {
            driver->logPhysicsState("Pendulum:angleNonFinite");
            return;
        }

        PhysicsSystem::tick(h);
        if (!std::isfinite(angle) || !std::isfinite(dAngle)) {
            driver->logPhysicsState("Pendulum:stateAfterTickNonFinite");
            return;
        }

        dBob = Vec2{-std::sin(angle), std::cos(angle)};
        if (!isFiniteVec(dBob)) {
            driver->logPhysicsState("Pendulum:unitVectorNonFinite");
            return;
        }
        float lengthVal = driver->getLength();
        if (!std::isfinite(lengthVal) || std::fabs(lengthVal) <= std::numeric_limits<float>::epsilon()) {
            driver->logPhysicsState("Pendulum:lengthInvalidInTick");
            return;
        }
        bob = Vec2{driver->anchor.x + dBob.x * lengthVal, driver->anchor.y + dBob.y * lengthVal};
        if (!isFiniteVec(bob)) {
            driver->logPhysicsState("Pendulum:bobNonFinite");
            return;
        }

        driver->output = bob;
    }

    void updateAnchor() override {
        const auto anchor = driver->anchor;
        const auto length = driver->getLength();
        bob = Vec2{anchor.x + 0.0f, anchor.y + length};
    }

    void drawDebugNative(const Mat4& trans = Mat4::identity()) override {
        Vec3Array points(common::SharedArray<Vec3>{{driver->anchor.x, driver->anchor.y, 0}, {bob.x, bob.y, 0}});
        core::dbg::inDbgSetBuffer(points);
        core::dbg::inDbgLineWidth(3);
        core::dbg::inDbgDrawLines(Vec4{1, 0, 1, 1}, trans);
    }

private:
    SimplePhysicsDriver* driver{};
    Vec2 bob{};
    float angle{0.0f};
    float dAngle{0.0f};
};

class SpringPendulumSystem : public PhysicsSystem {
public:
    explicit SpringPendulumSystem(SimplePhysicsDriver* d) : driver(d) {
        if (driver) {
            const auto anchor = driver->anchor;
            const auto length = driver->getLength();
            bob = Vec2{anchor.x + 0.0f, anchor.y + length};
        } else {
            bob = Vec2{0, 0};
        }
        addVariableNative(&bob);
        addVariableNative(&dBob);
        dBob = {0, 0};
    }

    void eval(float /*t*/) override {
        if (!driver) return;
        setD(0, dBob);

        float frequencyVal = driver->getFrequency();
        if (!std::isfinite(frequencyVal) || std::fabs(frequencyVal) <= std::numeric_limits<float>::epsilon()) {
            driver->logPhysicsState("SpringPendulum:frequencyInvalid");
            setD(2, Vec2{0, 0});
            return;
        }

        float springKsqrt = static_cast<float>(frequencyVal * 2.0f * detail::physicsPi);
        if (!std::isfinite(springKsqrt)) {
            driver->logPhysicsState("SpringPendulum:springKsqrtInvalid");
            setD(2, Vec2{0, 0});
            return;
        }

        float springK = springKsqrt * springKsqrt;
        if (!std::isfinite(springK) || std::fabs(springK) <= std::numeric_limits<float>::epsilon()) {
            driver->logPhysicsState("SpringPendulum:springKInvalid");
            setD(2, Vec2{0, 0});
            return;
        }

        float g = driver->getGravity();
        if (!std::isfinite(g)) {
            driver->logPhysicsState("SpringPendulum:gravityInvalid");
            setD(2, Vec2{0, 0});
            return;
        }

        float lengthVal = driver->getLength();
        if (!std::isfinite(lengthVal) || std::fabs(lengthVal) <= std::numeric_limits<float>::epsilon()) {
            driver->logPhysicsState("SpringPendulum:lengthInvalid");
            setD(2, Vec2{0, 0});
            return;
        }

        float restLength = lengthVal - g / springK;
        if (!std::isfinite(restLength)) {
            driver->logPhysicsState("SpringPendulum:restLengthInvalid");
            setD(2, Vec2{0, 0});
            return;
        }

        Vec2 offPos{bob.x - driver->anchor.x, bob.y - driver->anchor.y};
        if (!isFiniteVec(offPos)) {
            driver->logPhysicsState("SpringPendulum:offPosNonFinite");
            setD(2, Vec2{0, 0});
            return;
        }
        float offLen = std::sqrt(offPos.x * offPos.x + offPos.y * offPos.y);
        Vec2 offPosNorm{};
        if (!std::isfinite(offLen)) {
            driver->logPhysicsState("SpringPendulum:offLenNonFinite");
            setD(2, Vec2{0, 0});
            return;
        } else if (offLen <= std::numeric_limits<float>::epsilon()) {
            offPosNorm = isFiniteVec(lastValidOffPosNorm) ? lastValidOffPosNorm : Vec2{0.0f, 1.0f};
        } else {
            offPosNorm = Vec2{offPos.x / offLen, offPos.y / offLen};
            if (!isFiniteVec(offPosNorm)) {
                driver->logPhysicsState("SpringPendulum:offPosNormNonFinite");
                setD(2, Vec2{0, 0});
                return;
            }
            lastValidOffPosNorm = offPosNorm;
        }

        float lengthRatio = g / lengthVal;
        if (!std::isfinite(lengthRatio) || lengthRatio < 0) {
            driver->logPhysicsState("SpringPendulum:lengthRatioInvalid");
            setD(2, Vec2{0, 0});
            return;
        }

        float critDampAngle = 2.0f * std::sqrt(lengthRatio);
        float critDampLength = 2.0f * springKsqrt;
        if (!std::isfinite(critDampAngle) || !std::isfinite(critDampLength)) {
            driver->logPhysicsState("SpringPendulum:critDampInvalid");
            setD(2, Vec2{0, 0});
            return;
        }

        float dist = std::fabs(offLen);
        if (!std::isfinite(dist)) {
            driver->logPhysicsState("SpringPendulum:distanceInvalid");
            setD(2, Vec2{0, 0});
            return;
        }
        Vec2 force{0, g};
        force.x -= offPosNorm.x * (dist - restLength) * springK;
        force.y -= offPosNorm.y * (dist - restLength) * springK;
        Vec2 ddBob = force;
        if (!isFiniteVec(ddBob)) {
            driver->logPhysicsState("SpringPendulum:ddBobInvalid");
            ddBob = Vec2{0, 0};
        }

        Vec2 dBobRot{
            dBob.x * offPosNorm.y + dBob.y * offPosNorm.x,
            dBob.y * offPosNorm.y - dBob.x * offPosNorm.x,
        };

        float angleDampingVal = driver->getAngleDamping();
        float lengthDampingVal = driver->getLengthDamping();
        if (!std::isfinite(angleDampingVal) || !std::isfinite(lengthDampingVal)) {
            driver->logPhysicsState("SpringPendulum:dampingInvalid");
            setD(2, Vec2{0, 0});
            return;
        }

        Vec2 ddBobRot{
            -dBobRot.x * angleDampingVal * critDampAngle,
            -dBobRot.y * lengthDampingVal * critDampLength,
        };
        if (!isFiniteVec(ddBobRot)) {
            driver->logPhysicsState("SpringPendulum:ddBobRotInvalid");
            ddBobRot = Vec2{0, 0};
        }

        Vec2 ddBobDamping{
            ddBobRot.x * offPosNorm.y - dBobRot.y * offPosNorm.x,
            ddBobRot.y * offPosNorm.y + dBobRot.x * offPosNorm.x,
        };
        if (!isFiniteVec(ddBobDamping)) {
            driver->logPhysicsState("SpringPendulum:ddBobDampingInvalid");
            ddBobDamping = Vec2{0, 0};
        }

        ddBob.x += ddBobDamping.x;
        ddBob.y += ddBobDamping.y;
        if (!isFiniteVec(ddBob)) {
            driver->logPhysicsState("SpringPendulum:ddBobAfterDampingInvalid");
            ddBob = Vec2{0, 0};
        }

        setD(2, ddBob);
    }

    void tick(float h) {
        PhysicsSystem::tick(h);
        if (!driver) return;
        if (!isFiniteVec(bob) || !isFiniteVec(dBob)) {
            driver->logPhysicsState("SpringPendulum:stateAfterTickNonFinite");
            return;
        }
        if (!isFiniteVec(driver->anchor)) {
            driver->logPhysicsState("SpringPendulum:anchorNonFinite");
            return;
        }
        driver->output = bob;
    }

    void updateAnchor() override {
        const auto anchor = driver->anchor;
        const auto length = driver->getLength();
        bob = Vec2{anchor.x + 0.0f, anchor.y + length};
    }

    void drawDebugNative(const Mat4& trans = Mat4::identity()) override {
        Vec3Array points(common::SharedArray<Vec3>{{driver->anchor.x, driver->anchor.y, 0}, {bob.x, bob.y, 0}});
        core::dbg::inDbgSetBuffer(points);
        core::dbg::inDbgLineWidth(3);
        core::dbg::inDbgDrawLines(Vec4{1, 0, 1, 1}, trans);
    }

private:
    SimplePhysicsDriver* driver{};
    Vec2 bob{};
    Vec2 dBob{};
    Vec2 lastValidOffPosNorm{0.0f, 1.0f};
};

SimplePhysicsDriver::~SimplePhysicsDriver() = default;

SimplePhysicsDriver::SimplePhysicsDriver(DeferredConstruction) : Driver() {}

SimplePhysicsDriver::SimplePhysicsDriver() {
    reset();
}

std::shared_ptr<SimplePhysicsDriver> SimplePhysicsDriver::create(const std::shared_ptr<Node>& parent) {
    const auto actualParent = parent;
    ngRegisterModelResourceClassNames();
    auto result = std::allocate_shared<SimplePhysicsDriver>(
        d_runtime::SourceAllocator<SimplePhysicsDriver>{}, DeferredConstruction{});
    result->constructSimplePhysicsOwned(actualParent);
    return result;
}

std::shared_ptr<SimplePhysicsDriver> SimplePhysicsDriver::allocateSourcePhysicsReceiver() {
    ngRegisterModelResourceClassNames();
    return std::allocate_shared<SimplePhysicsDriver>(
        d_runtime::SourceAllocator<SimplePhysicsDriver>{}, DeferredConstruction{});
}

SimplePhysicsDriver::SimplePhysicsDriver(uint32_t uuidVal, const std::shared_ptr<Node>& parent)
    : Driver(uuidVal, parent) {
    reset();
}

common::NullableString SimplePhysicsDriver::typeId() const {
    return common::NullableString::fromStaticLiteral("SimplePhysics");
}

void SimplePhysicsDriver::runBeginTask(core::RenderContext& ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    Driver::runBeginTask(ctx);
    offsetGravity = 1.0f;
    offsetLength = 0.0f;
    offsetFrequency = 1.0f;
    offsetAngleDamping = 1.0f;
    offsetLengthDamping = 1.0f;
    offsetOutputScale = {1.0f, 1.0f};
}

void SimplePhysicsDriver::runPreProcessTask(core::RenderContext& ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    Vec4 prevPos = (localOnly ? [this] {
        const auto translation = transformLocal().translation;
        return Vec4{translation.x, translation.y, translation.z, 1.0f};
    }()
                              : transformPhysicsPoint(transform().toMat4(), Vec3{0, 0, 0}));
    Driver::runPreProcessTask(ctx);
    Vec4 anchorPos = (localOnly ? [this] {
        const auto translation = transformLocal().translation;
        return Vec4{translation.x, translation.y, translation.z, 1.0f};
    }()
                                : transformPhysicsPoint(transform().toMat4(), Vec3{0, 0, 0}));
    if (!std::isfinite(anchorPos.x) || !std::isfinite(anchorPos.y)) {
        logPhysicsState("preProcess:anchorNonFinite");
        return;
    }
    if (anchorPos.x != prevPos.x || anchorPos.y != prevPos.y) {
        anchor = Vec2{anchorPos.x, anchorPos.y};
        prevTransMat = transform().toMat4().inverse();
        prevAnchorSet = true;
    }
}

void SimplePhysicsDriver::runPostTaskImpl(std::size_t id, core::RenderContext& ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    Vec4 prevPos = (localOnly ? [this] {
        const auto translation = transformLocal().translation;
        return Vec4{translation.x, translation.y, translation.z, 1.0f};
    }()
                              : transformPhysicsPoint(transform().toMat4(), Vec3{0, 0, 0}));
    Driver::runPostTaskImpl(id, ctx);
    Vec4 anchorPos = (localOnly ? [this] {
        const auto translation = transformLocal().translation;
        return Vec4{translation.x, translation.y, translation.z, 1.0f};
    }()
                                : transformPhysicsPoint(transform().toMat4(), Vec3{0, 0, 0}));
    if (!std::isfinite(anchorPos.x) || !std::isfinite(anchorPos.y)) {
        logPhysicsState("postProcess:anchorNonFinite");
        return;
    }
    if (anchorPos.x != prevPos.x || anchorPos.y != prevPos.y) {
        anchor = Vec2{anchorPos.x, anchorPos.y};
        prevTransMat = transform().toMat4().inverse();
        prevAnchorSet = true;
    }
}

void SimplePhysicsDriver::serializeSelfImpl(::nicxlive::core::serde::InochiSerializer& serializer, bool recursive, SerializeNodeFlags flags) const {
    Node::serializeSelfImpl(serializer, recursive, flags);
    if (has_flag(flags, SerializeNodeFlags::State)) {
        serializer.putKey("param");
        serializer.putValue(static_cast<std::size_t>(paramRef));
        serializer.putKey("model_type");
        serializer.putValue(static_cast<int>(modelType.knownTag().value()));
        serializer.putKey("map_mode");
        serializer.putValue(static_cast<int>(mapMode.knownTag().value()));
        serializer.putKey("gravity");
        serializer.putValue(gravity);
        serializer.putKey("length");
        serializer.putValue(length);
        serializer.putKey("frequency");
        serializer.putValue(frequency);
        serializer.putKey("angle_damping");
        serializer.putValue(angleDamping);
        serializer.putKey("length_damping");
        serializer.putValue(lengthDamping);
        serializer.putKey("output_scale_x");
        serializer.putValue(outputScale[0]);
        serializer.putKey("output_scale_y");
        serializer.putValue(outputScale[1]);
        serializer.putKey("local_only");
        serializer.putValue(localOnly);
    }
}

::nicxlive::core::serde::SerdeException SimplePhysicsDriver::deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data) {
    auto err = Node::deserializeFromFghj(data);
    const bool hasModelType = data.get_child_optional("model_type").has_value();
    const bool hasMapMode = data.get_child_optional("map_mode").has_value();
    const bool hasOutputScale = data.get_child_optional("output_scale").has_value();
    if (auto p = data.get_optional<std::size_t>("param")) paramRef = static_cast<uint32_t>(*p);
    if (auto mt = data.get_optional<int>("model_type")) modelType = static_cast<PhysicsModel>(*mt);
    if (auto mts = data.get_optional<std::string>("model_type")) parsePhysicsModel(*mts, modelType);
    if (auto mm = data.get_optional<int>("map_mode")) mapMode = static_cast<ParamMapMode>(*mm);
    if (auto mms = data.get_optional<std::string>("map_mode")) parseParamMapMode(*mms, mapMode);
    if (auto g = data.get_optional<float>("gravity")) gravity = *g;
    if (auto l = data.get_optional<float>("length")) length = *l;
    if (auto f = data.get_optional<float>("frequency")) frequency = *f;
    if (auto ad = data.get_optional<float>("angle_damping")) angleDamping = *ad;
    if (auto ld = data.get_optional<float>("length_damping")) lengthDamping = *ld;
    if (auto os = data.get_child_optional("output_scale")) {
        std::size_t idx = 0;
        for (const auto& kv : *os) {
            if (idx >= 2) break;
            try {
                outputScale[idx] = kv.second.get_value<float>();
                ++idx;
            } catch (...) {
            }
        }
        if (auto x = os->get_optional<float>("x")) outputScale[0] = *x;
        if (auto y = os->get_optional<float>("y")) outputScale[1] = *y;
        if (auto x0 = os->get_optional<float>("0")) outputScale[0] = *x0;
        if (auto y1 = os->get_optional<float>("1")) outputScale[1] = *y1;
    }
    if (auto osx = data.get_optional<float>("output_scale_x")) outputScale[0] = *osx;
    if (auto osy = data.get_optional<float>("output_scale_y")) outputScale[1] = *osy;
    if (auto lo = data.get_optional<bool>("local_only")) localOnly = *lo;
    static int sDeserLog = 0;
    if (sDeserLog < 40) {
        NJCX_DBG_LOG("[nicxlive][SimplePhysics][deser] uuid=%u name=%s paramRef=%u model=%d map=%d len=%.6f out=(%.6f,%.6f) localOnly=%d\n",
                     uuid, name.c_str(), paramRef,
                     static_cast<int>(modelType.knownTag().value()), static_cast<int>(mapMode.knownTag().value()),
                     length, outputScale[0], outputScale[1], localOnly ? 1 : 0);
        ++sDeserLog;
    }
    reset();
    return err;
}

bool SimplePhysicsDriver::hasParam(const std::string& key) const {
    if (Driver::hasParam(key)) return true;
    return key == "gravity" ||
           key == "length" ||
           key == "frequency" ||
           key == "angleDamping" ||
           key == "lengthDamping" ||
           key == "outputScale.x" ||
           key == "outputScale.y";
}

float SimplePhysicsDriver::getDefaultValue(const std::string& key) const {
    if (Driver::hasParam(key)) return Driver::getDefaultValue(key);
    if (key == "gravity" ||
        key == "frequency" ||
        key == "angleDamping" ||
        key == "lengthDamping" ||
        key == "outputScale.x" ||
        key == "outputScale.y") {
        return 1.0f;
    }
    if (key == "length") return 0.0f;
    return 0.0f;
}

bool SimplePhysicsDriver::setValue(const std::string& key, float value) {
    if (Driver::setValue(key, value)) return true;
    if (!std::isfinite(value)) return false;
    if (key == "gravity") {
        offsetGravity *= value;
        return true;
    }
    if (key == "length") {
        offsetLength += value;
        return true;
    }
    if (key == "frequency") {
        offsetFrequency *= value;
        return true;
    }
    if (key == "angleDamping") {
        offsetAngleDamping *= value;
        return true;
    }
    if (key == "lengthDamping") {
        offsetLengthDamping *= value;
        return true;
    }
    if (key == "outputScale.x") {
        offsetOutputScale[0] *= value;
        return true;
    }
    if (key == "outputScale.y") {
        offsetOutputScale[1] *= value;
        return true;
    }
    return false;
}

float SimplePhysicsDriver::getValue(const std::string& key) const {
    if (key == "gravity") return offsetGravity;
    if (key == "length") return offsetLength;
    if (key == "frequency") return offsetFrequency;
    if (key == "angleDamping") return offsetAngleDamping;
    if (key == "lengthDamping") return offsetLengthDamping;
    if (key == "outputScale.x") return offsetOutputScale[0];
    if (key == "outputScale.y") return offsetOutputScale[1];
    return Driver::getValue(key);
}

bool SimplePhysicsDriver::hasParamSource(d_runtime::RetainedString sourceKey) {
    const auto self = shared_from_this();
    const auto key = sourceKey.view();
    if (Driver::hasParamSource(sourceKey)) return true;
    return key == "gravity" ||
           key == "length" ||
           key == "frequency" ||
           key == "angleDamping" ||
           key == "lengthDamping" ||
           key == "outputScale.x" ||
           key == "outputScale.y";
}

float SimplePhysicsDriver::getDefaultValueSource(d_runtime::RetainedString sourceKey) {
    const auto self = shared_from_this();
    const auto key = sourceKey.view();
    const auto defaultValue = Driver::getDefaultValueSource(sourceKey);
    if (!std::isnan(defaultValue)) return defaultValue;
    if (key == "gravity" ||
        key == "frequency" ||
        key == "angleDamping" ||
        key == "lengthDamping" ||
        key == "outputScale.x" ||
        key == "outputScale.y") {
        return 1.0f;
    }
    if (key == "length") return 0.0f;
    return std::numeric_limits<float>::quiet_NaN();
}

bool SimplePhysicsDriver::setValueSource(d_runtime::RetainedString sourceKey, float value) {
    const auto self = shared_from_this();
    const auto key = sourceKey.view();
    if (Driver::setValueSource(sourceKey, value)) return true;
    if (key == "gravity") {
        offsetGravity *= value;
        return true;
    }
    if (key == "length") {
        offsetLength += value;
        return true;
    }
    if (key == "frequency") {
        offsetFrequency *= value;
        return true;
    }
    if (key == "angleDamping") {
        offsetAngleDamping *= value;
        return true;
    }
    if (key == "lengthDamping") {
        offsetLengthDamping *= value;
        return true;
    }
    if (key == "outputScale.x") {
        offsetOutputScale[0] *= value;
        return true;
    }
    if (key == "outputScale.y") {
        offsetOutputScale[1] *= value;
        return true;
    }
    return false;
}

float SimplePhysicsDriver::getValueSource(d_runtime::RetainedString sourceKey) {
    const auto self = shared_from_this();
    const auto key = sourceKey.view();
    if (key == "gravity") return offsetGravity;
    if (key == "length") return offsetLength;
    if (key == "frequency") return offsetFrequency;
    if (key == "angleDamping") return offsetAngleDamping;
    if (key == "lengthDamping") return offsetLengthDamping;
    if (key == "outputScale.x") return offsetOutputScale[0];
    if (key == "outputScale.y") return offsetOutputScale[1];
    return Driver::getValueSource(sourceKey);
}

std::shared_ptr<core::param::Parameter> SimplePhysicsDriver::resolveParam() {
    return param_;
}

void SimplePhysicsDriver::updateDriver() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    float h = static_cast<float>(std::min(deltaTime(), 10.0));
    updateInputs();
    while (h > 0.01) {
        d_runtime::ngRequireObject(system)->tick(0.01f);
        h = static_cast<float>(h - 0.01);
    }
    d_runtime::ngRequireObject(system)->tick(h);
    updateOutputs();
    prevAnchorSet = false;
}

void SimplePhysicsDriver::updateAnchors() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    d_runtime::ngRequireObject(system)->updateAnchor();
}

void SimplePhysicsDriver::reset() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    updateInputs();
    offsetGravity = 1.0f;
    offsetLength = 0.0f;
    offsetFrequency = 1.0f;
    offsetAngleDamping = 1.0f;
    offsetLengthDamping = 1.0f;
    offsetOutputScale = {1.0f, 1.0f};

    const auto selectedModel = getModelType();
    const auto owner = std::dynamic_pointer_cast<SimplePhysicsDriver>(weak_from_this().lock());
    switch (selectedModel.knownTag().value_or(static_cast<PhysicsModel>(-1))) {
    case PhysicsModel::Pendulum:
        if (owner) system = Pendulum::create(owner);
        else system = std::allocate_shared<PendulumSystem>(d_runtime::SourceAllocator<PendulumSystem>{}, this);
        break;
    case PhysicsModel::SpringPendulum:
        if (owner) system = SpringPendulum::create(owner);
        else system = std::allocate_shared<SpringPendulumSystem>(
            d_runtime::SourceAllocator<SpringPendulumSystem>{}, this);
        break;
    default:
        throw d_runtime::ngStaticError<d_runtime::AssertError>(
            d_runtime::RetainedString::fromStaticLiteral("Assertion failure"),
            d_runtime::RetainedString::fromStaticLiteral("source/nijilive/core/nodes/drivers/simplephysics.d"), 857);
    }
}

void SimplePhysicsDriver::updateInputs() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (prevAnchorSet) return;
    Vec4 anchorPos = localOnly ? [this] {
        const auto translation = transformLocal().translation;
        return Vec4{translation.x, translation.y, translation.z, 1.0f};
    }()
                               : transformPhysicsPoint(transform().toMat4(), Vec3{0, 0, 0});
    if (!std::isfinite(anchorPos.x) || !std::isfinite(anchorPos.y)) {
        logPhysicsState("updateInputs:anchorNonFinite");
        return;
    }
    anchor = Vec2{anchorPos.x, anchorPos.y};
}

void SimplePhysicsDriver::updateOutputs() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (!getParam()) return;

    if (!std::isfinite(output.x) || !std::isfinite(output.y)) {
        logPhysicsState("updateOutputs:outputNonFinite");
        return;
    }
    if (!std::isfinite(anchor.x) || !std::isfinite(anchor.y)) {
        logPhysicsState("updateOutputs:anchorNonFinite");
        return;
    }

    Vec2 oscale = getOutputScale();
    if (!std::isfinite(oscale.x) || !std::isfinite(oscale.y)) {
        logPhysicsState("updateOutputs:scaleNonFinite");
        return;
    }

    Vec4 localPos = localOnly
        ? Vec4{output.x, output.y, 0.0f, 1.0f}
        : transformPhysicsPoint(prevAnchorSet ? prevTransMat : transform().toMat4().inverse(),
            Vec3{output.x, output.y, 0.0f});
    Vec2 localAngle{localPos.x, localPos.y};
    if (!std::isfinite(localAngle.x) || !std::isfinite(localAngle.y)) {
        logPhysicsState("updateOutputs:localPosNonFinite");
        return;
    }

    float localAngleLen = std::sqrt(localAngle.x * localAngle.x + localAngle.y * localAngle.y);
    if (!std::isfinite(localAngleLen) || std::fabs(localAngleLen) <= std::numeric_limits<float>::epsilon()) {
        logPhysicsState("updateOutputs:angleLengthInvalid");
        return;
    }
    localAngle.x /= localAngleLen;
    localAngle.y /= localAngleLen;

    float lengthVal = getLength();
    if (!std::isfinite(lengthVal) || std::fabs(lengthVal) <= std::numeric_limits<float>::epsilon()) {
        logPhysicsState("updateOutputs:lengthInvalid");
        return;
    }
    float distanceVal = std::sqrt((output.x - anchor.x) * (output.x - anchor.x) + (output.y - anchor.y) * (output.y - anchor.y));
    if (!std::isfinite(distanceVal)) {
        logPhysicsState("updateOutputs:distanceInvalid");
        return;
    }
    float relLength = distanceVal / lengthVal;
    if (!std::isfinite(relLength)) {
        logPhysicsState("updateOutputs:relLengthInvalid");
        return;
    }

    Vec2 paramVal{};
    switch (mapMode.knownTag().value_or(static_cast<ParamMapMode>(-1))) {
    case ParamMapMode::XY: {
        Vec2 localPosNorm{localAngle.x * relLength, localAngle.y * relLength};
        paramVal = Vec2{localPosNorm.x - 0.0f, localPosNorm.y - 1.0f};
        paramVal.y = -paramVal.y;
        break;
    }
    case ParamMapMode::AngleLength: {
        float a = static_cast<float>(std::atan2(-localAngle.x, localAngle.y) / detail::physicsPi);
        paramVal = Vec2{a, relLength};
        break;
    }
    case ParamMapMode::YX: {
        Vec2 localPosNorm{localAngle.x * relLength, localAngle.y * relLength};
        paramVal = Vec2{localPosNorm.x - 0.0f, localPosNorm.y - 1.0f};
        paramVal.y = -paramVal.y;
        paramVal = Vec2{paramVal.y, paramVal.x};
        break;
    }
    case ParamMapMode::LengthAngle: {
        float a = static_cast<float>(std::atan2(-localAngle.x, localAngle.y) / detail::physicsPi);
        paramVal = Vec2{relLength, a};
        break;
    }
    default:
        throw d_runtime::ngStaticError<d_runtime::AssertError>(
            d_runtime::RetainedString::fromStaticLiteral("Assertion failure"),
            d_runtime::RetainedString::fromStaticLiteral("source/nijilive/core/nodes/drivers/simplephysics.d"), 814);
    }

    if (!std::isfinite(paramVal.x) || !std::isfinite(paramVal.y)) {
        logPhysicsState("updateOutputs:paramValInvalid");
        return;
    }

    Vec2 paramOffset{paramVal.x * oscale.x, paramVal.y * oscale.y};
    if (!std::isfinite(paramOffset.x) || !std::isfinite(paramOffset.y)) {
        logPhysicsState("updateOutputs:paramOffsetNonFinite");
        return;
    }
    d_runtime::ngRequireObject(getParam())->pushIOffset(paramOffset, core::param::ParamMergeMode::Forced);
    d_runtime::ngRequireObject(getParam())->update();
}

void SimplePhysicsDriver::finalize() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    param_ = d_runtime::ngRequireObject(puppetRef())->findParameter(paramRef);
    paramCached = param_;
    Driver::finalize();
    reset();
}

void SimplePhysicsDriver::logPhysicsState(std::string_view, std::string_view) {}

void SimplePhysicsDriver::drawDebug() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    d_runtime::ngRequireObject(system)->drawDebug();
}

float SimplePhysicsDriver::getGravity() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    const float relativeGravity = gravity * offsetGravity;
    const auto puppet = d_runtime::ngRequireObject(puppetRef());
    const float globalGravity = d_runtime::ngRequireObject(puppet->physics)->gravity;
    const float scaledGravity = relativeGravity * globalGravity;
    return scaledGravity * getScale();
}
float SimplePhysicsDriver::getLength() { return length + offsetLength; }
float SimplePhysicsDriver::getFrequency() { return frequency * offsetFrequency; }
float SimplePhysicsDriver::getAngleDamping() { return angleDamping * offsetAngleDamping; }
float SimplePhysicsDriver::getLengthDamping() { return lengthDamping * offsetLengthDamping; }
Vec2 SimplePhysicsDriver::getOutputScale() {
    return Vec2{outputScale[0] * offsetOutputScale[0], outputScale[1] * offsetOutputScale[1]};
}
float SimplePhysicsDriver::getScale() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    return d_runtime::ngRequireObject(d_runtime::ngRequireObject(puppetRef())->physics)->pixelsPerMeter;
}

float SimplePhysicsDriver::getGravity() const {
    float puppetGravity = 9.8f;
    float pixelsPerMeter = 1000.0f;
    if (auto pup = puppetRef()) {
        puppetGravity = pup->physics->gravity;
        pixelsPerMeter = pup->physics->pixelsPerMeter;
    }
    return (gravity * offsetGravity) * puppetGravity * pixelsPerMeter;
}
float SimplePhysicsDriver::getLength() const { return length + offsetLength; }
float SimplePhysicsDriver::getFrequency() const { return frequency * offsetFrequency; }
float SimplePhysicsDriver::getAngleDamping() const { return angleDamping * offsetAngleDamping; }
float SimplePhysicsDriver::getLengthDamping() const { return lengthDamping * offsetLengthDamping; }
Vec2 SimplePhysicsDriver::getOutputScale() const { return Vec2{outputScale[0] * offsetOutputScale[0], outputScale[1] * offsetOutputScale[1]}; }

std::vector<std::shared_ptr<core::param::Parameter>> SimplePhysicsDriver::getAffectedParameters() const {
    if (!param_) return {};
    return {param_};
}

bool SimplePhysicsDriver::affectsParameter(const std::shared_ptr<core::param::Parameter>& p) const {
    if (!p) return false;
    for (auto& ap : getAffectedParameters()) {
        if (ap && ap->uuid == p->uuid) return true;
    }
    return false;
}

void SimplePhysicsDriver::serializeSelfImpl(::nicxlive::core::serde::TypedJsonWriter& serializer,
    bool recursive, SerializeNodeFlags flags) const {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    Driver::serializeSelfImpl(serializer, recursive, flags);
    // These fields are unconditional in D, even when State is absent.
    serializer.putField("param", paramRef);
    serializer.putKey("model_type");
    switch (modelType.knownTag().value_or(static_cast<PhysicsModel>(-1))) {
    case PhysicsModel::Pendulum: serializer.putValue("Pendulum"); break;
    case PhysicsModel::SpringPendulum: serializer.putValue("SpringPendulum"); break;
    default:
        throw d_runtime::ngStaticError<d_runtime::AssertError>(
            d_runtime::RetainedString::fromStaticLiteral("Assertion failure"),
            d_runtime::RetainedString::fromStaticLiteral("mir/serde.d"), 522);
    }
    serializer.putKey("map_mode");
    switch (mapMode.knownTag().value_or(static_cast<ParamMapMode>(-1))) {
    case ParamMapMode::AngleLength: serializer.putValue("AngleLength"); break;
    case ParamMapMode::XY: serializer.putValue("XY"); break;
    case ParamMapMode::LengthAngle: serializer.putValue("LengthAngle"); break;
    case ParamMapMode::YX: serializer.putValue("YX"); break;
    default:
        throw d_runtime::ngStaticError<d_runtime::AssertError>(
            d_runtime::RetainedString::fromStaticLiteral("Assertion failure"),
            d_runtime::RetainedString::fromStaticLiteral("mir/serde.d"), 522);
    }
    serializer.putField("gravity", gravity);
    serializer.putField("length", length);
    serializer.putField("frequency", frequency);
    serializer.putField("angle_damping", angleDamping);
    serializer.putField("length_damping", lengthDamping);
    serializer.putField("output_scale", outputScale);
    serializer.putField("local_only", localOnly);
}

::nicxlive::core::serde::ReadError SimplePhysicsDriver::deserializeFromFghj(
    ::nicxlive::core::serde::TypedJsonView data) {
    namespace serde = ::nicxlive::core::serde;
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    // Returned base errors are ignored; thrown exceptions still escape.
    (void)Driver::deserializeFromFghj(data);
    auto read = [&](const char* key, auto& value) -> serde::ReadError {
        const auto field = data.findFirst(key);
        if (field.isMissing()) return {};
        return serde::deserializeValue(field, value);
    };
    if (auto error = read("param", paramRef)) return error;
    const auto model = data.findFirst("model_type");
    if (!model.isMissing()) deserializePhysicsStringEnum(model, modelType_);
    const auto mode = data.findFirst("map_mode");
    if (!mode.isMissing()) deserializePhysicsStringEnum(mode, mapMode);
    if (auto error = read("gravity", gravity)) return error;
    if (auto error = read("length", length)) return error;
    if (auto error = read("frequency", frequency)) return error;
    if (auto error = read("angle_damping", angleDamping)) return error;
    if (auto error = read("length_damping", lengthDamping)) return error;
    const auto scale = data.findFirst("output_scale");
    if (!scale.isMissing())
        if (auto error = serde::deserializeVector(scale, std::span<float>(outputScale))) return error;
    if (auto error = read("local_only", localOnly)) return error;
    return {};
}

} // namespace nicxlive::core::nodes
