#pragma once
#include <cstdint>

namespace CleanVR {
struct Vec { float x, y, z; };
struct Mat { float m[4][4]; };
struct Quat { float x, y, z, w; };
Vec Add(Vec, Vec); Vec Sub(Vec, Vec); Vec Scale(Vec, float);
float Dot(Vec, Vec); Vec Cross(Vec, Vec); float Length(Vec); Vec Unit(Vec);
Mat Identity(); Mat Mul(const Mat&, const Mat&); Mat Inverse(const Mat&);
Vec Position(const Mat&); Vec Row(const Mat&, int); void SetPosition(Mat&, Vec);
Vec Point(Vec, const Mat&); Vec Direction(Vec, const Mat&);
bool Rigid(const Mat&); bool Near(const Mat&, const Mat&, float positionTolerance = .001f, float rotationTolerance = .001f);
Mat FromOpenXR(Vec position, Quat orientation); // RH OpenXR -> LH BF2; row vectors
Mat Level(const Mat&); Mat Yaw(float radians);
Mat LevelInRig(const Mat& nativeCamera,const Mat& bodyWorld);
// First-person fallback shoulder frame. The connected 3P solve uses the
// model's own shoulder joints instead of these synthetic arm roots.
Mat ShoulderFrame(const Mat& trackedHead,const Mat& torso);
constexpr float ShoulderHalfWidth=.18f;
Mat Compose(const Mat& nativeCamera, const Mat& reference, const Mat& tracked, float unitsPerMeter);
bool Projection(float left, float right, float up, float down, float nearZ, float farZ, Mat& out);
Mat Facing(const Mat&, Vec); Mat RotateBetween(Vec, Vec);
// Shortest-arc rigid interpolation for body and shared supported-aim filters.
Mat BlendRigidPose(const Mat&,const Mat&,float fraction);

struct HandBinding {
    Mat right, left, rightCandidate, leftCandidate;
    std::uint64_t started = 0, lastSeen = 0, rightSince = 0, leftSince = 0;
    bool valid = false, rightReady = false, leftReady = false, rightPending = false, leftPending = false;
    void Update(const Mat bones[70], std::uint64_t now);
};
bool PalmBinding(const Mat bones[70], int wrist, Mat& wristFromGrip);
struct HandTargets {
    Mat head, torso, rightGrip, rightAim, leftGrip, leftAim;
    Mat rightPalm, leftPalm;
    bool rightValid = false, leftValid = false, palmsValid = false;
    bool supportPressed = false, wasSupporting = false;
    std::uint64_t frame = 0, origin = 0;
    std::int64_t time = 0;
    // A pump stroke slides the support hand; retain the captured gun attitude
    // relative to the current right controller until the stroke closes.
    bool pumpPose = false;
    Mat pumpAimFromRight{};
    float pumpAimWeight = 0;
};
// Stored in the level player frame, never in the moving eye or weapon frame.
// Each OpenXR sample advances once; body/launch consumers use a snapshot.
struct HandAimState {
    Mat pose{};
    std::uint64_t frame = 0, origin = 0;
    std::int64_t time = 0;
    bool valid = false;
};
// Pure solve. Roots 0/1 stay native; fingers retain authored local articulation.
bool SolveHands(const Mat native[70], const HandBinding&, const HandTargets&, Mat result[70], bool& supporting, HandAimState* aimState=nullptr);
// Visual contact only: never used as controller input or to move the rifle.
bool SolveLeftContact(const Mat native[70],const HandTargets&,const Mat contact[21],Mat result[70]);
// Stock connected 80-bone body; palm targets are in this rig's coordinates.
// Shoulders stay attached to the torso; only the two arm branches change.
// Visual elbow/forearm history; controller wrists and weapon poses stay exact.
struct BodyArmSmoothing { Vec pole[2]{};float twist[2][2]{},seconds=0;bool valid[2]{}; };
struct BodyArmStats { float reach[2]{},stretch[2]{},roll[2]{},swivel[2]{};unsigned folded=0;int rejected=0; };
// Optional wrist positions join the connected sleeves to actual 1P hands.
// Rotation still uses the 3P bone axes derived from its anatomical palm.
bool SolveBodyArms(const Mat native[80],const Mat& leftPalm,const Mat& rightPalm,const Mat& torso,Mat result[80],BodyArmStats& stats,const Vec* wristPositions=nullptr,float leftContactWeight=0,BodyArmSmoothing* smoothing=nullptr,float leftElbowWeight=0);
bool MapLaunch(const Mat& nativeLaunch, const Mat& nativeCamera, const Mat& nativeWeapon,
               const Mat& trackedWeapon, bool weaponSpace, Mat& out);
}
