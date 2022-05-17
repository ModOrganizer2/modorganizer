#ifndef MODORGANIZER_SANITYCHECKS_INCLUDED
#define MODORGANIZER_SANITYCHECKS_INCLUDED

namespace env
{
class Environment;
class Module;
}  // namespace env

namespace MOBase
{
class IPluginGame;
}

class Settings;

namespace sanity
{

void checkEnvironment(const env::Environment& env);
int checkIncompatibleModule(const env::Module& m);
int checkPaths(MOBase::IPluginGame& game, const Settings& s);

}  // namespace sanity

#endif  // MODORGANIZER_SANITYCHECKS_INCLUDED
