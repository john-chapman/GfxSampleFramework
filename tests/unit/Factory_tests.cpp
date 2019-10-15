#include <catch.hpp>

#include <apt/log.h>
#include <apt/Factory.h>

using namespace apt;

struct Entity: public Factory<Entity>
{
};
APT_FACTORY_DEFINE(Entity);

struct Player: public Entity
{
};
APT_FACTORY_REGISTER_DEFAULT(Entity, Player);

struct Enemy: public Entity
{
	static Entity* Create()              { return new Enemy(); }
	static void Destroy(Entity*& _inst_) { delete _inst_; }
};
APT_FACTORY_REGISTER(Entity, Enemy, Enemy::Create, Enemy::Destroy);


TEST_CASE("Factory registration", "[Factory]")
{
	REQUIRE(Entity::GetClassRefCount() == 2);
}
TEST_CASE("Factory Create/Destroy", "[Factory]")
{
	Entity* player = Entity::Create("Player");
	REQUIRE(player->getClassRef()->getNameHash() == StringHash("Player"));
	Entity* enemy = Entity::Create("Enemy");
	REQUIRE(enemy->getClassRef()->getNameHash() == StringHash("Enemy"));
	
	Entity::Destroy(player);
	REQUIRE(player == nullptr);
	Entity::Destroy(enemy);
	REQUIRE(enemy == nullptr);
}