#include "simple_physics_driver.hpp"
#include "../serde.hpp"
#include "../param/parameter.hpp"
#include "../puppet.hpp"
#include "../nodes/common.hpp"
#include "../timing.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <limits>
#include <numbers>
#include <vector>

namespace nicxlive::core::nodes {

namespace {
float deltaTime() {
    return static_cast<float>(::nicxlive::core::deltaTime());
}

bool isFiniteVec(const Vec2& v) {
    return std::isfinite(v.x) && std::isfinite(v.y);
}

bool parsePhysicsModel(const std::string& value, PhysicsModel& out) {
    if (value == "pendulum") {
        out = PhysicsModel::Pendulum;
        return true;
    }
    if (value == "spring_pendulum") {
        out = PhysicsModel::SpringPendulum;
        return true;
    }
    return false;
}

bool parseParamMapMode(const std::string& value, ParamMapMode& out) {
    if (value == "angle_length") {
        out = ParamMapMode::AngleLength;
        return true;
    }
    if (value == "xy") {
        out = ParamMapMode::XY;
        return true;
    }
    if (value == "length_angle") {
        out = ParamMapMode::LengthAngle;
        return true;
    }
    if (value == "yx") {
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

// RK4 integrator that supports floats flattened from vec2 variables.
class PhysicsSystem {
public:
    virtual ~PhysicsSystem() = default;
    virtual void eval(float t) = 0;
    virtual void updateAnchor() = 0;
    virtual void drawDebug(const Mat4& trans = Mat4::identity()) = 0;

    void addVariable(float* v) { refs.push_back(v); }
    void addVariable(Vec2* v) {
        refs.push_back(&v->x);
        refs.push_back(&v->y);
    }
    void setD(std::size_t idx, float v) {
        if (idx >= derivative.size()) derivative.resize(refs.size(), 0.0f);
        derivative[idx] = v;
    }
    void setD(std::size_t idx, const Vec2& v) {
        if (idx + 1 >= derivative.size()) derivative.resize(refs.size(), 0.0f);
        derivative[idx] = v.x;
        derivative[idx + 1] = v.y;
    }

    virtual void tick(float h) {
        if (refs.empty()) return;
        derivative.assign(refs.size(), 0.0f);

        std::vector<float> cur(refs.size(), 0.0f);
        for (std::size_t i = 0; i < refs.size(); ++i) cur[i] = *refs[i];

        std::vector<float> k1(refs.size()), k2(refs.size()), k3(refs.size()), k4(refs.size());

        eval(t_);
        k1 = derivative;

        for (std::size_t i = 0; i < refs.size(); ++i) *refs[i] = cur[i] + h * k1[i] * 0.5f;
        eval(t_ + h * 0.5f);
        k2 = derivative;

        for (std::size_t i = 0; i < refs.size(); ++i) *refs[i] = cur[i] + h * k2[i] * 0.5f;
        eval(t_ + h * 0.5f);
        k3 = derivative;

        for (std::size_t i = 0; i < refs.size(); ++i) *refs[i] = cur[i] + h * k3[i];
        eval(t_ + h);
        k4 = derivative;

        for (std::size_t i = 0; i < refs.size(); ++i) {
            float next = cur[i] + h * (k1[i] + 2 * k2[i] + 2 * k3[i] + k4[i]) / 6.0f;
            if (!std::isfinite(next)) {
                for (std::size_t j = 0; j < refs.size(); ++j) *refs[j] = cur[j];
                return;
            }
            *refs[i] = next;
        }

        t_ += h;
    }

protected:
    float t_{0.0f};
    std::vector<float*> refs{};
    std::vector<float> derivative{};
};

class PendulumSystem : public PhysicsSystem {
public:
    explicit PendulumSystem(SimplePhysicsDriver* d) : driver(d) {
        addVariable(&angle);
        addVariable(&dAngle);
        if (driver) {
            bob = Vec2{driver->anchor.x, driver->anchor.y + driver->getLength()};
        } else {
            bob = Vec2{0, 0};
        }
    }

    void eval(float /*t*/) override {
        if (!driver) return;
        derivative.assign(refs.size(), 0.0f);
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
        bob = Vec2{driver->anchor.x, driver->anchor.y + driver->getLength()};
    }

    void drawDebug(const Mat4& trans = Mat4::identity()) override {
        Vec3 a{driver->anchor.x, driver->anchor.y, 0};
        Vec3 b{bob.x, bob.y, 0};
        auto& buf = debugDrawBuffer();
        buf.push_back(DebugLine{a, b, Vec4{1, 0, 1, 1}});
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
        addVariable(&bob);
        addVariable(&dBob);
        if (driver) {
            bob = Vec2{driver->anchor.x, driver->anchor.y + driver->getLength()};
        } else {
            bob = Vec2{0, 0};
        }
        dBob = {0, 0};
    }

    void eval(float /*t*/) override {
        if (!driver) return;
        derivative.assign(refs.size(), 0.0f);
        setD(0, dBob);

        float frequencyVal = driver->getFrequency();
        if (!std::isfinite(frequencyVal) || std::fabs(frequencyVal) <= std::numeric_limits<float>::epsilon()) {
            driver->logPhysicsState("SpringPendulum:frequencyInvalid");
            setD(2, Vec2{0, 0});
            return;
        }

        float springKsqrt = frequencyVal * 2.0f * std::numbers::pi_v<float>;
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
        Vec2 offPosNorm = (offLen > std::numeric_limits<float>::epsilon()) ? Vec2{offPos.x / offLen, offPos.y / offLen} : Vec2{0, 0};
        if (!isFiniteVec(offPosNorm)) {
            driver->logPhysicsState("SpringPendulum:offPosNormNonFinite");
            setD(2, Vec2{0, 0});
            return;
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
        bob = Vec2{driver->anchor.x, driver->anchor.y + driver->getLength()};
    }

    void drawDebug(const Mat4& trans = Mat4::identity()) override {
        Vec3 a{driver->anchor.x, driver->anchor.y, 0};
        Vec3 b{bob.x, bob.y, 0};
        auto& buf = debugDrawBuffer();
        buf.push_back(DebugLine{a, b, Vec4{1, 0, 1, 1}});
    }

private:
    SimplePhysicsDriver* driver{};
    Vec2 bob{};
    Vec2 dBob{};
};

SimplePhysicsDriver::~SimplePhysicsDriver() = default;

SimplePhysicsDriver::SimplePhysicsDriver() {
    requirePreProcessTask();
    requirePostTask(0);
}

SimplePhysicsDriver::SimplePhysicsDriver(uint32_t uuidVal, const std::shared_ptr<Node>& parent)
    : Driver(uuidVal, parent) {
    requirePreProcessTask();
    requirePostTask(0);
}

const std::string& SimplePhysicsDriver::typeId() const {
    static const std::string k = "SimplePhysics";
    return k;
}

void SimplePhysicsDriver::runBeginTask(core::RenderContext& ctx) {
    Driver::runBeginTask(ctx);
    offsetGravity = 1.0f;
    offsetLength = 0.0f;
    offsetFrequency = 1.0f;
    offsetAngleDamping = 1.0f;
    offsetLengthDamping = 1.0f;
    offsetOutputScale = {1.0f, 1.0f};
    resolveParam();
}

void SimplePhysicsDriver::runPreProcessTask(core::RenderContext& ctx) {
    Vec3 prevPos = (localOnly ? Vec3{transformLocal().translation.x, transformLocal().translation.y, 0.0f}
                              : transform().toMat4().transformPoint(Vec3{0, 0, 0}));
    Driver::runPreProcessTask(ctx);
    Vec3 anchorPos = (localOnly ? Vec3{transformLocal().translation.x, transformLocal().translation.y, 0.0f}
                                : transform().toMat4().transformPoint(Vec3{0, 0, 0}));
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
    Vec3 prevPos = (localOnly ? Vec3{transformLocal().translation.x, transformLocal().translation.y, 0.0f}
                              : transform().toMat4().transformPoint(Vec3{0, 0, 0}));
    Driver::runPostTaskImpl(id, ctx);
    Vec3 anchorPos = (localOnly ? Vec3{transformLocal().translation.x, transformLocal().translation.y, 0.0f}
                                : transform().toMat4().transformPoint(Vec3{0, 0, 0}));
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
        serializer.putValue(static_cast<int>(modelType));
        serializer.putKey("map_mode");
        serializer.putValue(static_cast<int>(mapMode));
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

std::shared_ptr<core::param::Parameter> SimplePhysicsDriver::resolveParam() {
    auto paramPtr = paramCached.lock();
    if (paramPtr) return paramPtr;
    if (param_) paramPtr = param_;
    if (!paramPtr && paramRef != 0) {
        if (auto pup = puppetRef()) {
            paramPtr = pup->findParameter(paramRef);
        }
    }
    if (paramPtr) {
        param_ = paramPtr;
        paramCached = paramPtr;
    }
    return paramPtr;
}

void SimplePhysicsDriver::updateDriver() {
    if (!system || systemModel != modelType) {
        if (modelType == PhysicsModel::SpringPendulum) {
            system = std::make_unique<SpringPendulumSystem>(this);
        } else {
            system = std::make_unique<PendulumSystem>(this);
        }
        systemModel = modelType;
    }

    if (!system) return;

    updateInputs();

    float h = std::min(deltaTime(), 10.0f);
    while (h > 0.01f) {
        system->tick(0.01f);
        h -= 0.01f;
    }
    system->tick(h);
    updateOutputs();
    prevAnchorSet = false;
}

void SimplePhysicsDriver::reset() {
    updateInputs();
    offsetGravity = 1.0f;
    offsetLength = 0.0f;
    offsetFrequency = 1.0f;
    offsetAngleDamping = 1.0f;
    offsetLengthDamping = 1.0f;
    offsetOutputScale = {1.0f, 1.0f};
    prevAnchor = {0, 0};
    prevAnchorSet = false;
    simPhase = 0.0f;
    output = {0, 0};
    angle = 0.0f;
    dAngle = 0.0f;
    lengthVel = 0.0f;

    switch (modelType) {
    case PhysicsModel::Pendulum:
        system = std::make_unique<PendulumSystem>(this);
        break;
    case PhysicsModel::SpringPendulum:
        system = std::make_unique<SpringPendulumSystem>(this);
        break;
    }
    systemModel = modelType;
}

void SimplePhysicsDriver::updateInputs() {
    if (prevAnchorSet) return;
    Vec3 anchorPos = localOnly ? Vec3{transformLocal().translation.x, transformLocal().translation.y, 0.0f}
                               : transform().toMat4().transformPoint(Vec3{0, 0, 0});
    if (!std::isfinite(anchorPos.x) || !std::isfinite(anchorPos.y)) {
        logPhysicsState("updateInputs:anchorNonFinite");
        return;
    }
    anchor = Vec2{anchorPos.x, anchorPos.y};
}

void SimplePhysicsDriver::updateOutputs() {
    auto paramPtr = resolveParam();
    if (!paramPtr) {
        static int sNoParamCount = 0;
        if (sNoParamCount < 40) {
            std::fprintf(stderr, "[nicxlive][SimplePhysics] no-param driver=%u name=%s paramRef=%u\n",
                         uuid, name.c_str(), paramRef);
            ++sNoParamCount;
        }
        return;
    }

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

    Vec3 localPos = localOnly
        ? Vec3{output.x, output.y, 0.0f}
        : (prevAnchorSet ? prevTransMat : transform().toMat4().inverse()).transformPoint(Vec3{output.x, output.y, 0.0f});
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
    switch (mapMode) {
    case ParamMapMode::XY: {
        Vec2 localPosNorm{localAngle.x * relLength, localAngle.y * relLength};
        paramVal = Vec2{localPosNorm.x, 1.0f - localPosNorm.y};
        break;
    }
    case ParamMapMode::AngleLength: {
        float a = std::atan2(-localAngle.x, localAngle.y) / std::numbers::pi_v<float>;
        paramVal = Vec2{a, relLength};
        break;
    }
    case ParamMapMode::YX: {
        Vec2 localPosNorm{localAngle.x * relLength, localAngle.y * relLength};
        paramVal = Vec2{1.0f - localPosNorm.y, localPosNorm.x};
        break;
    }
    case ParamMapMode::LengthAngle: {
        float a = std::atan2(-localAngle.x, localAngle.y) / std::numbers::pi_v<float>;
        paramVal = Vec2{relLength, a};
        break;
    }
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
    static int sTraceCount = 0;
    if (sTraceCount < 80) {
        std::fprintf(stderr,
                     "[nicxlive][SimplePhysics] push driver=%u name=%s param=%u offset=(%.6f,%.6f) out=(%.6f,%.6f) anchor=(%.6f,%.6f)\n",
                     uuid, name.c_str(), paramPtr->uuid,
                     paramOffset.x, paramOffset.y, output.x, output.y, anchor.x, anchor.y);
        ++sTraceCount;
    }

    paramPtr->pushIOffset(paramOffset, core::param::ParamMergeMode::Forced);
    paramPtr->update();
}

void SimplePhysicsDriver::finalize() {
    if (auto pup = puppetRef()) {
        param_ = pup->findParameter(paramRef);
        paramCached = param_;
    } else {
        param_.reset();
        paramCached.reset();
    }
    Driver::finalize();
    reset();
}

void SimplePhysicsDriver::logPhysicsState(const std::string& context, const std::string& extra) {
    std::cerr << "[SimplePhysicsDriver] " << context;
    if (!extra.empty()) std::cerr << " " << extra;
    std::cerr << std::endl;
}

void SimplePhysicsDriver::drawDebug() {
    if (system) system->drawDebug();
}

float SimplePhysicsDriver::getGravity() const {
    float puppetGravity = 9.8f;
    float pixelsPerMeter = 1000.0f;
    if (auto pup = puppetRef()) {
        puppetGravity = pup->physics.gravity;
        pixelsPerMeter = pup->physics.pixelsPerMeter;
    }
    return (gravity * offsetGravity) * puppetGravity * pixelsPerMeter;
}
float SimplePhysicsDriver::getLength() const { return length + offsetLength; }
float SimplePhysicsDriver::getFrequency() const { return frequency * offsetFrequency; }
float SimplePhysicsDriver::getAngleDamping() const { return angleDamping * offsetAngleDamping; }
float SimplePhysicsDriver::getLengthDamping() const { return lengthDamping * offsetLengthDamping; }
Vec2 SimplePhysicsDriver::getOutputScale() const { return Vec2{outputScale[0] * offsetOutputScale[0], outputScale[1] * offsetOutputScale[1]}; }

std::vector<std::shared_ptr<core::param::Parameter>> SimplePhysicsDriver::getAffectedParameters() const {
    std::vector<std::shared_ptr<core::param::Parameter>> out;
    std::shared_ptr<core::param::Parameter> p;
    if (auto cached = paramCached.lock()) {
        p = cached;
    } else if (param_) {
        p = param_;
    } else if (paramRef != 0) {
        if (auto pup = puppetRef()) {
            p = pup->findParameter(paramRef);
        }
    }
    if (p) out.push_back(p);
    return out;
}

bool SimplePhysicsDriver::affectsParameter(const std::shared_ptr<core::param::Parameter>& p) const {
    if (!p) return false;
    for (auto& ap : getAffectedParameters()) {
        if (ap && ap->uuid == p->uuid) return true;
    }
    return false;
}

} // namespace nicxlive::core::nodes
