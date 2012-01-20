#include <unistd.h>
#include <v8.h>
#include <node.h>
#include <node_object_wrap.h>
using namespace v8;
using namespace node;

namespace Library {
class Inventory {
public:
    Inventory() : items(0) {}

    void addStock(int n) {
        items += n;
    }

    int ship(int orders) {
        if (orders > items)
            return -1;

        items -= orders;
        return 0;
    }

    int getItems() {
        return items;
    }

    void reshelve() {
        sleep(5);
    }

private:
    int items;
};
}

namespace binding {

class Inventory;
struct ReshelveBaton {
    uv_work_t request;
    Persistent<Function> callback;
    Inventory *wrapper;
    // any other data that has to be sent to the callback
    // or for async processing.
};

class Inventory : public ObjectWrap {
public:
    Inventory() : ObjectWrap()
                , inv(new Library::Inventory()) {};

    ~Inventory() { delete inv; }

    static Handle<Value> New(const Arguments &args) {
        Inventory *wrapper = new Inventory();
        wrapper->Wrap(args.Holder());
        return args.Holder();
    }

    static Handle<Value> AddStock(const Arguments &args) {
        Inventory *wrapper = Unwrap<Inventory>(args.Holder());
        int n = args[0]->Uint32Value();

        wrapper->inv->addStock(n);
        return Undefined();
    }

    static Handle<Value> Ship(const Arguments &args) {
        Inventory *wrapper = Unwrap<Inventory>(args.Holder());
        int orders = args[0]->Uint32Value();
        int result = wrapper->inv->ship(orders);

        if (result == -1)
            return ThrowException(String::New("Not enough items"));

        return Undefined();
    }

    static Handle<Value> GetItems(Local<String> property, const AccessorInfo &info) {
        Inventory *wrapper = Unwrap<Inventory>(info.Holder());
        return Integer::New(wrapper->inv->getItems());
    }

    static Handle<Value> Reshelve(const Arguments &args) {
        Inventory *wrapper = Unwrap<Inventory>(args.Holder());

        Handle<Function> cb = Handle<Function>::Cast(args[0]);

        ReshelveBaton *baton = new ReshelveBaton();
        baton->request.data = baton;
        baton->callback = Persistent<Function>::New(cb);
        baton->wrapper = wrapper;

        uv_queue_work(Loop(), &baton->request,
                      ReshelveAsync, ReshelveAsyncAfter);

        return Undefined();
    }

    static void ReshelveAsync(uv_work_t *req) {
        // This runs in a separate thread
        // NO V8 interaction should be done
        ReshelveBaton *baton =
            static_cast<ReshelveBaton*>(req->data);
        // if you want to modify baton values
        // do synchronous work here
        baton->wrapper->inv->reshelve();
    }

    static void ReshelveAsyncAfter(uv_work_t *req) {
        ReshelveBaton *baton =
            static_cast<ReshelveBaton*>(req->data);

        baton->callback->Call(Context::GetCurrent()->Global(),
                              0, NULL);

        baton->callback.Dispose();
        delete baton;
    }

private:
    Library::Inventory *inv;
};

extern "C" {
    static void Init(Handle<Object> target) {
        HandleScope scope;

        Handle<FunctionTemplate> inventoryTpl =
            FunctionTemplate::New(Inventory::New);

        Handle<ObjectTemplate> instance =
            inventoryTpl->InstanceTemplate();

        instance->SetInternalFieldCount(1);

        instance->SetAccessor(String::New("items"), Inventory::GetItems);

        NODE_SET_PROTOTYPE_METHOD(inventoryTpl, "addStock", Inventory::AddStock);
        NODE_SET_PROTOTYPE_METHOD(inventoryTpl, "ship", Inventory::Ship);
        NODE_SET_PROTOTYPE_METHOD(inventoryTpl, "reshelve", Inventory::Reshelve);

        target->Set(String::NewSymbol("Inventory"),
                    inventoryTpl->GetFunction());
    }
    NODE_MODULE(async, Init)
}
}
