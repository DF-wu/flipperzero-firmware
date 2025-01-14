#!/usr/bin/env python3

from flipper.storage import FlipperStorage
import logging
import argparse
import os
import sys
import binascii
import posixpath


class Main:
    def __init__(self):
        # command args
        self.parser = argparse.ArgumentParser()
        self.parser.add_argument("-d", "--debug", action="store_true", help="Debug")
        self.parser.add_argument("-p", "--port", help="CDC Port", required=True)
        self.subparsers = self.parser.add_subparsers(help="sub-command help")

        self.parser_mkdir = self.subparsers.add_parser("mkdir", help="Create directory")
        self.parser_mkdir.add_argument("flipper_path", help="Flipper path")
        self.parser_mkdir.set_defaults(func=self.mkdir)

        self.parser_remove = self.subparsers.add_parser(
            "remove", help="Remove file/directory"
        )
        self.parser_remove.add_argument("flipper_path", help="Flipper path")
        self.parser_remove.set_defaults(func=self.remove)

        self.parser_read = self.subparsers.add_parser("read", help="Read file")
        self.parser_read.add_argument("flipper_path", help="Flipper path")
        self.parser_read.set_defaults(func=self.read)

        self.parser_size = self.subparsers.add_parser("size", help="Size of file")
        self.parser_size.add_argument("flipper_path", help="Flipper path")
        self.parser_size.set_defaults(func=self.size)

        self.parser_receive = self.subparsers.add_parser("receive", help="Receive file")
        self.parser_receive.add_argument("flipper_path", help="Flipper path")
        self.parser_receive.add_argument("local_path", help="Local path")
        self.parser_receive.set_defaults(func=self.receive)

        self.parser_send = self.subparsers.add_parser(
            "send", help="Send file or directory"
        )
        self.parser_send.add_argument(
            "-f", "--force", help="Force sending", action="store_true"
        )
        self.parser_send.add_argument("local_path", help="Local path")
        self.parser_send.add_argument("flipper_path", help="Flipper path")
        self.parser_send.set_defaults(func=self.send)

        self.parser_list = self.subparsers.add_parser(
            "list", help="Recursively list files and dirs"
        )
        self.parser_list.add_argument("flipper_path", help="Flipper path", default="/")
        self.parser_list.set_defaults(func=self.list)

        # logging
        self.logger = logging.getLogger()

    def __call__(self):
        self.args = self.parser.parse_args()
        if "func" not in self.args:
            self.parser.error("Choose something to do")
        # configure log output
        self.log_level = logging.DEBUG if self.args.debug else logging.INFO
        self.logger.setLevel(self.log_level)
        self.handler = logging.StreamHandler(sys.stdout)
        self.handler.setLevel(self.log_level)
        self.formatter = logging.Formatter("%(asctime)s [%(levelname)s] %(message)s")
        self.handler.setFormatter(self.formatter)
        self.logger.addHandler(self.handler)
        # execute requested function
        self.args.func()

    def mkdir(self):
        storage = FlipperStorage(self.args.port)
        storage.start()
        self.logger.debug(f'Creating "{self.args.flipper_path}"')
        if not storage.mkdir(self.args.flipper_path):
            self.logger.error(f"Error: {storage.last_error}")
        storage.stop()

    def remove(self):
        storage = FlipperStorage(self.args.port)
        storage.start()
        self.logger.debug(f'Removing "{self.args.flipper_path}"')
        if not storage.remove(self.args.flipper_path):
            self.logger.error(f"Error: {storage.last_error}")
        storage.stop()

    def receive(self):
        storage = FlipperStorage(self.args.port)
        storage.start()

        if storage.exist_dir(self.args.flipper_path):
            for dirpath, dirnames, filenames in storage.walk(self.args.flipper_path):
                self.logger.debug(
                    f'Processing directory "{os.path.normpath(dirpath)}"'.replace(
                        os.sep, "/"
                    )
                )
                dirnames.sort()
                filenames.sort()

                rel_path = os.path.relpath(dirpath, self.args.flipper_path)

                for dirname in dirnames:
                    local_dir_path = os.path.join(
                        self.args.local_path, rel_path, dirname
                    )
                    local_dir_path = os.path.normpath(local_dir_path)
                    os.makedirs(local_dir_path, exist_ok=True)

                for filename in filenames:
                    local_file_path = os.path.join(
                        self.args.local_path, rel_path, filename
                    )
                    local_file_path = os.path.normpath(local_file_path)
                    flipper_file_path = os.path.normpath(
                        os.path.join(dirpath, filename)
                    ).replace(os.sep, "/")
                    self.logger.info(
                        f'Receiving "{flipper_file_path}" to "{local_file_path}"'
                    )
                    if not storage.receive_file(flipper_file_path, local_file_path):
                        self.logger.error(f"Error: {storage.last_error}")

        else:
            self.logger.info(
                f'Receiving "{self.args.flipper_path}" to "{self.args.local_path}"'
            )
            if not storage.receive_file(self.args.flipper_path, self.args.local_path):
                self.logger.error(f"Error: {storage.last_error}")
        storage.stop()

    def send(self):
        storage = FlipperStorage(self.args.port)
        storage.start()
        self.send_to_storage(
            storage, self.args.flipper_path, self.args.local_path, self.args.force
        )
        storage.stop()

    # send file or folder recursively
    def send_to_storage(self, storage, flipper_path, local_path, force):
        if not os.path.exists(local_path):
            self.logger.error(f'Error: "{local_path}" is not exist')

        if os.path.isdir(local_path):
            # create parent dir
            self.mkdir_on_storage(storage, flipper_path)

            for dirpath, dirnames, filenames in os.walk(local_path):
                self.logger.debug(f'Processing directory "{os.path.normpath(dirpath)}"')
                dirnames.sort()
                filenames.sort()
                rel_path = os.path.relpath(dirpath, local_path)

                # create subdirs
                for dirname in dirnames:
                    flipper_dir_path = os.path.join(flipper_path, rel_path, dirname)
                    flipper_dir_path = os.path.normpath(flipper_dir_path).replace(
                        os.sep, "/"
                    )
                    self.mkdir_on_storage(storage, flipper_dir_path)

                # send files
                for filename in filenames:
                    flipper_file_path = os.path.join(flipper_path, rel_path, filename)
                    flipper_file_path = os.path.normpath(flipper_file_path).replace(
                        os.sep, "/"
                    )
                    local_file_path = os.path.normpath(os.path.join(dirpath, filename))
                    self.send_file_to_storage(
                        storage, flipper_file_path, local_file_path, force
                    )
        else:
            self.send_file_to_storage(storage, flipper_path, local_path, force)

    # make directory with exist check
    def mkdir_on_storage(self, storage, flipper_dir_path):
        if not storage.exist_dir(flipper_dir_path):
            self.logger.debug(f'"{flipper_dir_path}" not exist, creating')
            if not storage.mkdir(flipper_dir_path):
                self.logger.error(f"Error: {storage.last_error}")
        else:
            self.logger.debug(f'"{flipper_dir_path}" already exist')

    # send file with exist check and hash check
    def send_file_to_storage(self, storage, flipper_file_path, local_file_path, force):
        if not storage.exist_file(flipper_file_path):
            self.logger.debug(
                f'"{flipper_file_path}" not exist, sending "{local_file_path}"'
            )
            self.logger.info(f'Sending "{local_file_path}" to "{flipper_file_path}"')
            if not storage.send_file(local_file_path, flipper_file_path):
                self.logger.error(f"Error: {storage.last_error}")
        elif force:
            self.logger.debug(
                f'"{flipper_file_path}" exist, but will be overwritten by "{local_file_path}"'
            )
            self.logger.info(f'Sending "{local_file_path}" to "{flipper_file_path}"')
            if not storage.send_file(local_file_path, flipper_file_path):
                self.logger.error(f"Error: {storage.last_error}")
        else:
            self.logger.debug(
                f'"{flipper_file_path}" exist, compare hash with "{local_file_path}"'
            )
            hash_local = storage.hash_local(local_file_path)
            hash_flipper = storage.hash_flipper(flipper_file_path)

            if not hash_flipper:
                self.logger.error(f"Error: {storage.last_error}")

            if hash_local == hash_flipper:
                self.logger.debug(
                    f'"{flipper_file_path}" are equal to "{local_file_path}"'
                )
            else:
                self.logger.debug(
                    f'"{flipper_file_path}" are not equal to "{local_file_path}"'
                )
                self.logger.info(
                    f'Sending "{local_file_path}" to "{flipper_file_path}"'
                )
                if not storage.send_file(local_file_path, flipper_file_path):
                    self.logger.error(f"Error: {storage.last_error}")

    def read(self):
        storage = FlipperStorage(self.args.port)
        storage.start()
        self.logger.debug(f'Reading "{self.args.flipper_path}"')
        data = storage.read_file(self.args.flipper_path)
        if not data:
            self.logger.error(f"Error: {storage.last_error}")
        else:
            try:
                print("Text data:")
                print(data.decode())
            except:
                print("Binary hexadecimal data:")
                print(binascii.hexlify(data).decode())
        storage.stop()

    def size(self):
        storage = FlipperStorage(self.args.port)
        storage.start()
        self.logger.debug(f'Getting size of "{self.args.flipper_path}"')
        size = storage.size(self.args.flipper_path)
        if size < 0:
            self.logger.error(f"Error: {storage.last_error}")
        else:
            print(size)
        storage.stop()

    def list(self):
        storage = FlipperStorage(self.args.port)
        storage.start()
        self.logger.debug(f'Listing "{self.args.flipper_path}"')
        storage.list_tree(self.args.flipper_path)
        storage.stop()


if __name__ == "__main__":
    Main()()
