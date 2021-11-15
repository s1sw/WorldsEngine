using System;
using Xunit;
using WorldsEngine.ECS;

namespace WorldsEngine.Tests
{
    [Component]
    public class TestComponent
    {
        public int Value;
    }

    [Component]
    public class ThinkingComponent : IThinkingComponent
    {
        public bool HasThought = false;

        public void Think(Entity e)
        {
            HasThought = true;
        }
    }

    public class ComponentStorageTests
    {
        [Fact]
        public void TestAddRemoveHasComponent()
        {
            ComponentStorage<TestComponent> cs = new();
            Entity ent = new(0);
            cs.Set(ent, new TestComponent());
            Assert.True(cs.Contains(ent));
            cs.Remove(ent);
            Assert.False(cs.Contains(ent));
        }

        [Fact]
        public void ThrowsOnNonExistentGet()
        {
            ComponentStorage<TestComponent> cs = new();
            Assert.Throws<ArgumentException>(() => cs.Get(new Entity(0)));
        }

        [Fact]
        public void ThrowsOnNonExistentRemove()
        {
            ComponentStorage<TestComponent> cs = new();
            Assert.Throws<ArgumentException>(() => cs.Remove(new Entity(0)));
        }

        [Fact]
        public void TestDataRetrieval()
        {
            ComponentStorage<TestComponent> cs = new();
            cs.Set(new Entity(0), new TestComponent() { Value = 5 });
            var tc = cs.Get(new Entity(0));
            Assert.Equal(5, tc.Value);
        }

        [Fact]
        public void ThinkingComponent()
        {
            ComponentStorage<ThinkingComponent> cs = new();
            cs.Set(new Entity(), new ThinkingComponent());
            cs.UpdateIfThinking();
            Assert.True(cs.Get(new Entity()).HasThought);
        }
    }
}
