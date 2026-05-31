#include "MessageHandlerRegistry.h"

MessageHandlerRegistry& MessageHandlerRegistry::Instance() {
	static MessageHandlerRegistry s_instance;
	return s_instance;
}

void MessageHandlerRegistry::Register(const MessageType::Game id, Factory factory) {
	m_Factories[id] = std::move(factory);
}

const MessageHandlerRegistry::Factory* MessageHandlerRegistry::Lookup(const MessageType::Game id) const {
	const auto it = m_Factories.find(id);
	return it == m_Factories.end() ? nullptr : &it->second;
}

size_t MessageHandlerRegistry::Size() const {
	return m_Factories.size();
}

void MessageHandlerRegistry::ClearForTesting() {
	m_Factories.clear();
}
